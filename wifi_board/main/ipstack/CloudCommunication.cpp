#include "CloudCommunication.h"
#include <sstream>
#include <unordered_map>
#include <optional>
#include <cstdlib>
#include <cerrno>
#include <charconv>
#include <string_view>
#include <memory>

static const char *TAG = "CloudCommunication";

template <typename T>
struct normalize_type { using type = T; };

template <> struct normalize_type<std::string> { using type = std::string_view; };
template <> struct normalize_type<const char*> { using type = std::string_view; };
template <size_t N> struct normalize_type<char[N]> { using type = std::string_view; };
template <size_t N> struct normalize_type<const char[N]> { using type = std::string_view; };

template <typename T>
using normalize_type_t = typename normalize_type<std::decay_t<T>>::type;

template <typename T_return, typename T_param>
struct EnumTraits;

template <>
struct EnumTraits<commands, std::string_view> {
    static const inline std::unordered_map<std::string_view, commands> map = {
        { "TOGGLE_PLUG", commands::TOGGLE_PLUG },
        { "PLUG_ON", commands::PLUG_ON },
        { "PLUG_OFF", commands::PLUG_OFF },
        { "OPEN_NETWORK", commands::OPEN_NETWORK }
    };
};

template <>
struct EnumTraits<std::string_view, data_type_t> {
    static const inline std::unordered_map<data_type_t, std::string_view> map = {
        { data_type_t::DATA_TYPE_DEVICE_JOIN, "DATA_TYPE_DEVICE_JOIN" },
        { data_type_t::DATA_TYPE_DEVICE_LEFT, "DATA_TYPE_DEVICE_LEFT" },
        { data_type_t::DATA_TYPE_POWER, "DATA_TYPE_POWER" },
        { data_type_t::DATA_TYPE_ENERGY, "DATA_TYPE_ENERGY" },
        { data_type_t::DATA_TYPE_VOLTAGE, "DATA_TYPE_VOLTAGE" },
        { data_type_t::DATA_TYPE_CURRENT, "DATA_TYPE_CURRENT" },
        { data_type_t::DATA_TYPE_SET_ON, "DATA_TYPE_SET_ON" },
        { data_type_t::DATA_TYPE_PRIORITY, "DATA_TYPE_PRIORITY" },
        { data_type_t::DATA_TYPE_ONLINE_STATE, "DATA_TYPE_ONLINE_STATE" },
    };
};

template <typename T_return, typename T_param>
static std::optional<T_return> convertEnum(const T_param& param) {
    using NormReturn = normalize_type_t<T_return>;
    using NormParam  = normalize_type_t<T_param>;

    const auto& map = EnumTraits<NormReturn, NormParam>::map;    
    NormParam lookup_key = param;

    if (auto it = map.find(lookup_key); it != map.end()) {
        return it->second;
    }
    return std::nullopt;
}

template <typename T_split>
static std::vector<T_split> split(const std::string& str, char delimiter) {
    std::vector<T_split> tokens;
    std::string token;
    std::stringstream ss(str);

    while (std::getline(ss, token, delimiter)) {
        if constexpr (std::is_same_v<T_split, std::string>) {
            tokens.push_back(token);
        } else {
            T_split value;
            std::stringstream token_ss(token);
            if (token_ss >> value) {
                tokens.push_back(value);
            }
        }
    }

    return tokens;
}


CloudCommunication::CloudCommunication(IPStack *_ipstack, EventGroupHandle_t _wifi_eg,
    QueueHandle_t _rx_queue, QueueHandle_t _tx_queue)
: ipstack(_ipstack), wifi_eg(_wifi_eg), rx_queue(_rx_queue), tx_queue(_tx_queue)
{
    uint8_t mac[6];
    if (get_efuse_mac(mac)) {
        snprintf(efuse_mac, sizeof(efuse_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        char jwt_buffer[512] = {0};
        int jwt_error = generate_jwt(jwt_buffer, sizeof(jwt_buffer),
            DEVICE_JWT_SECRET, efuse_mac);
        if (jwt_error == 0) {
            auth_headers.insert(
                { "Authorization", std::string("Bearer ") + jwt_buffer }
            );
        }
    }

    std::ostringstream ws_url_ss;
    ws_url_ss << "ws://" << API_HOSTNAME
            << ":" << WS_PORT
            << "/ws?hub=" << efuse_mac;
    ws_url = ws_url_ss.str();

    elec_price_req_timer_h = xTimerCreate("ELEC_PRICE_REQ", pdMS_TO_TICKS(15 * MINUTE_TO_MS), pdTRUE,
        static_cast<void*>(this), elec_price_req_timer_cb);

    xTaskCreate(cloud_task, "CLOUD_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 2, &cloud_task_handle);
}

CloudCommunication::~CloudCommunication() {
    if (elec_price_req_timer_h) xTimerDelete(elec_price_req_timer_h, portMAX_DELAY);
    if (cloud_task_handle) vTaskDelete(cloud_task_handle);
}

void CloudCommunication::elec_price_req_timer_cb(TimerHandle_t xTimer)
{
    auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
    xEventGroupSetBits(cloud_communication->wifi_eg, GET_ELEC_PRICE_EVENT_BIT);
}

void CloudCommunication::cloud_task(void *param)
{
    ESP_LOGI(TAG, "Cloud task started");

    auto cloud_communication = static_cast<CloudCommunication*>(param);
    auto ipstack = cloud_communication->ipstack;

    xTimerStart(cloud_communication->elec_price_req_timer_h, 0);

    std::vector<float> price_vec;
    while (true) {
        EventBits_t wifi_bits = ipstack->get_wifi_bits(portMAX_DELAY);
        if (wifi_bits & WIFI_FAIL_BIT) {
            xEventGroupClearBits(cloud_communication->wifi_eg, WIFI_FAIL_BIT);
            cloud_communication->send_wifi_status(false);
            esp_err_t err = ipstack->deinit_websocket();
            ESP_LOGI(TAG, "Websocket stoped: %s", esp_err_to_name(err));
        }

        if (wifi_bits & WIFI_CONNECTED_BIT) {    
            EventBits_t event_bits = xEventGroupWaitBits(cloud_communication->wifi_eg,
                ON_WIFI_CONNECT_BIT | GET_ELEC_PRICE_EVENT_BIT,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(50)
            );
            if (event_bits & ON_WIFI_CONNECT_BIT) {
                ESP_LOGI(TAG, "Wifi connection detected.");
                cloud_communication->send_wifi_status(true);
                cloud_communication->validate_hub();
                cloud_communication->get_electricity_price(price_vec);
                cloud_communication->connect_websocket();
            }
            if (event_bits & GET_ELEC_PRICE_EVENT_BIT) {
                cloud_communication->get_electricity_price(price_vec);
            }
    
            cloud_communication->send_data();
            cloud_communication->parse_websocket_data();
        }
    }
}

void CloudCommunication::validate_hub()
{
    if (auth_headers.empty()) {
        ESP_LOGI(TAG, "No auth headers found");
        return;
    }
    auto buffer = std::unique_ptr<char, decltype(&std::free)>(
        static_cast<char*>(calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1)), 
        std::free
    );
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
        return;
    }

    bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer.get(),
        "/api/initial_log_to_db", "", "",
        HTTP_METHOD_POST, auth_headers);

    if (success) {
        ESP_LOGI(TAG, "Go to: http://%s:%d/register_hub\nAnd enter code: %s\nTo register hub.",
            API_HOSTNAME, API_PORT, efuse_mac);
    } else {
        ESP_LOGI(TAG, "Error :(");
    }
}

void CloudCommunication::send_data()
{
    if (auth_headers.empty()) {
        ESP_LOGI(TAG, "No auth headers found");
        return;
    }

    controller_data ctrl_data;
    if (xQueueReceive(rx_queue, &ctrl_data, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Sending data...");
        auto data_type_str = convertEnum<std::string_view>(ctrl_data.type);
        if (!data_type_str.has_value()) {
            ESP_LOGI(TAG, "Invalid datatype");
            return;
        }

        std::ostringstream send_http_body_ss;
        send_http_body_ss << "device_id=" << ctrl_data.device_id
                    << "&type=" << data_type_str.value()
                    << "&value=" << ctrl_data.data.value
                    << "&value_int=" << ctrl_data.data.value_int
                    << "&flag=" << ctrl_data.data.flag;
        auto send_http_body = send_http_body_ss.str();
        // ESP_LOGI(TAG, "%s: %s", pcName, send_http_body.c_str());

        auto buffer = std::unique_ptr<char, decltype(&std::free)>(
            static_cast<char*>(calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1)), 
            std::free
        );
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
            return;
        }
        auto headers = auth_headers;
        headers.insert({ "Content-Type", "application/x-www-form-urlencoded" });

        bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer.get(),
            "/api/send_device_data", "", send_http_body.c_str(), HTTP_METHOD_POST, headers);

        ESP_LOGI(TAG, "Data send %s", success ? "successull" : "failed");
    }
}

void CloudCommunication::get_electricity_price(std::vector<float> &price_vec)
{
    if (price_vec.empty()) {
        auto buffer = std::unique_ptr<char, decltype(&std::free)>(
            static_cast<char*>(calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1)), 
            std::free
        );
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
            return;
        }
        bool success = ipstack->http_request(API_HOSTNAME, API_PORT,
                buffer.get(), "/api/get_electricity_prices");

        if (success) {
            std::string prices = buffer.get();
            price_vec = split<float>(prices, ',');

            ESP_LOGI(TAG, "Got electricity prices for the next %d 15mins", price_vec.size());

            controller_data ctrl_data = {};
            ctrl_data.type = DATA_TYPE_ELEC_PRICE;
            ctrl_data.data.value = price_vec.back();

            // TODO: Pass data to UART
            if (xQueueSendToBack(tx_queue, &ctrl_data, portMAX_DELAY) == pdTRUE) {
                ESP_LOGI(TAG, "Electricity price updated to: %.2f", ctrl_data.data.value);
                price_vec.pop_back();
            }
        } else {
            ESP_LOGE(TAG, "Error getting electricity prices");
        }
    } else {
        controller_data ctrl_data = {};
        ctrl_data.type = DATA_TYPE_ELEC_PRICE;
        ctrl_data.data.value = price_vec.back();

        if (xQueueSendToBack(tx_queue, &ctrl_data, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Electricity price updated to: %.2f", ctrl_data.data.value);
            price_vec.pop_back();
        }
    }
}

void CloudCommunication::connect_websocket()
{
    if (ws_url.empty()) {
        ESP_LOGE(TAG, "Websocket url not found.");
        return;
    }

    esp_err_t err = ipstack->init_websocket(ws_url.c_str());
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "Failed to init websocket client: %s", esp_err_to_name(err));
        return;
    }
    EventBits_t bits = xEventGroupWaitBits(wifi_eg,
        WEBSOCKET_CONNECTED_BIT | WEBSOCKET_ERROR_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WEBSOCKET_NETWORK_TIMEOUT_MS)
    );
    if (bits & WEBSOCKET_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Websocket connected to %s", ws_url.c_str());
    } else if (bits & WEBSOCKET_ERROR_BIT) {
        ESP_LOGE(TAG, "Websocket error");
    } else {
        ESP_LOGI(TAG, "Websocket connect failed %s", ws_url.c_str());
    }
}

void CloudCommunication::parse_websocket_data()
{
    EventBits_t bits = xEventGroupGetBits(wifi_eg);
    if (bits & WEBSOCKET_ERROR_BIT) {
        return;
    }

    if (bits & WEBSOCKET_CONNECTED_BIT) {
        t_websocket_data ws_data = {};
        while (ipstack->get_websocket_data(&ws_data, 0)) {
            ESP_LOGI(TAG, "Websocket data: %s", ws_data.payload);
            std::string payload_str = ws_data.payload;
            auto parsed_cmd = split<std::string>(payload_str, '|');
            if (parsed_cmd.size() < 2) {
                ESP_LOGI(TAG, "No websocket data to process.");
                continue;
            }
        
            controller_data ctrl_data = { .type = DATA_TYPE_COMMAND };
            auto command = convertEnum<commands>(parsed_cmd[0]);
            if (command.has_value()) {
                ctrl_data.data.command = command.value();
            } else {
                ESP_LOGI(TAG, "Invalid command: %s", parsed_cmd[0].c_str());
                continue;
            }
        
            auto device_id_str = parsed_cmd[1];
            uint64_t device_id = 0;
            auto [ptr, ec] = std::from_chars(device_id_str.data(),
                device_id_str.data() + device_id_str.size(), device_id);
        
            if (ec == std::errc{}) {
                ctrl_data.device_id = device_id;
            } else {
                ESP_LOGI(TAG, "Invalid device_id: %s", device_id_str.c_str());
                continue;
            }
        
            if (xQueueSendToBack(tx_queue, &ctrl_data, 0) != pdTRUE) {
                ESP_LOGI(TAG, "Failed to send data to controller queue");
                continue;
            }
            ESP_LOGI(TAG, "Websocket data processed succesfully :)");
    
        }
        return;
    }
}

void CloudCommunication::send_wifi_status(bool online)
{
    controller_data ctrl_data = { .type = DATA_TYPE_WIFI_ONLINE };
    ctrl_data.data.flag = online;
    xQueueSendToBack(tx_queue, &ctrl_data, 0);
}
