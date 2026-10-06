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
struct EnumTraits<std::string_view, commands> {
    static const inline std::unordered_map<commands, std::string_view> map = {
        { commands::TOGGLE_PLUG, "TOGGLE_PLUG" },
        { commands::PLUG_ON, "PLUG_ON" },
        { commands::PLUG_OFF, "PLUG_OFF" },
        { commands::OPEN_NETWORK, "OPEN_NETWORK" }
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
        { data_type_t::DATA_TYPE_ELEC_PRICE, "DATA_TYPE_ELEC_PRICE" },
        { data_type_t::DATA_TYPE_THRESHOLD_MED, "DATA_TYPE_THRESHOLD_MED" },
        { data_type_t::DATA_TYPE_THRESHOLD_LOW, "DATA_TYPE_THRESHOLD_LOW" },
        { data_type_t::DATA_TYPE_DEVICE_NAME, "DATA_TYPE_DEVICE_NAME" }
    };
};

template <>
struct EnumTraits<data_type_t, std::string_view> {
    static const inline std::unordered_map<std::string_view, data_type_t> map = {
        { "DATA_TYPE_DEVICE_JOIN", data_type_t::DATA_TYPE_DEVICE_JOIN },
        { "DATA_TYPE_DEVICE_LEFT", data_type_t::DATA_TYPE_DEVICE_LEFT },
        { "DATA_TYPE_POWER", data_type_t::DATA_TYPE_POWER },
        { "DATA_TYPE_ENERGY", data_type_t::DATA_TYPE_ENERGY },
        { "DATA_TYPE_VOLTAGE", data_type_t::DATA_TYPE_VOLTAGE },
        { "DATA_TYPE_CURRENT", data_type_t::DATA_TYPE_CURRENT },
        { "DATA_TYPE_SET_ON", data_type_t::DATA_TYPE_SET_ON },
        { "DATA_TYPE_PRIORITY", data_type_t::DATA_TYPE_PRIORITY },
        { "DATA_TYPE_ONLINE_STATE", data_type_t::DATA_TYPE_ONLINE_STATE },
        { "DATA_TYPE_ELEC_PRICE", data_type_t::DATA_TYPE_ELEC_PRICE },
        { "DATA_TYPE_THRESHOLD_MED", data_type_t::DATA_TYPE_THRESHOLD_MED },
        { "DATA_TYPE_THRESHOLD_LOW", data_type_t::DATA_TYPE_THRESHOLD_LOW },
        { "DATA_TYPE_COMMAND", data_type_t::DATA_TYPE_COMMAND },
        { "DATA_TYPE_DEVICE_NAME", data_type_t::DATA_TYPE_DEVICE_NAME }
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

static bool try_string_to_float(std::string_view str, float& out_val) {
    // 1. Strip leading whitespace (std::from_chars does not skip whitespace)
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) {
        return false; // Empty or whitespace-only string
    }
    str.remove_prefix(first);

    // 2. Parse number
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out_val);

    // 3. Verify success (ec == std::errc{}) and complete parsing (ptr reached the end)
    return ec == std::errc{} && ptr == str.data() + str.size();
}

static bool try_string_to_int(std::string_view str, int& out_val, int base = 10) {
    // 1. Strip leading whitespace
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) return false;
    str.remove_prefix(first);

    // 2. Handle optional leading '+' (std::from_chars only handles '-')
    if (!str.empty() && str[0] == '+') {
        str.remove_prefix(1);
    }

    // 3. Parse integer
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out_val, base);

    // 4. Verify success and no trailing garbage
    return ec == std::errc{} && ptr == str.data() + str.size();
}

// template <typename T_conv>
// static bool try_string_to_num(const std::string& str, T_conv& out_val) {
//     std::istringstream iss(str);
//     T_conv val;

//     if ((iss >> val) && (iss >> std::ws).eof()) {
//         out_val = val;
//         return true;
//     }

//     return false;
// }


CloudCommunication::CloudCommunication(IPStack *_ipstack, EventGroupHandle_t _wifi_eg,
    QueueHandle_t _cloud_q, QueueHandle_t _controller_q)
: ipstack(_ipstack), wifi_eg(_wifi_eg), cloud_q(_cloud_q), controller_q(_controller_q)
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
    send_wifi_board_info_timer_h = xTimerCreate("SEND_WIFI_STATUS", pdMS_TO_TICKS(30 * 1000), pdTRUE,
        static_cast<void*>(this), send_wifi_board_info_timer_cb);

    xTaskCreate(cloud_task, "CLOUD_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 2, &cloud_task_handle);
}

CloudCommunication::~CloudCommunication() {
    if (elec_price_req_timer_h) xTimerDelete(elec_price_req_timer_h, portMAX_DELAY);
    if (send_wifi_board_info_timer_h) xTimerDelete(send_wifi_board_info_timer_h, portMAX_DELAY);
    if (cloud_task_handle) vTaskDelete(cloud_task_handle);
}

void CloudCommunication::elec_price_req_timer_cb(TimerHandle_t xTimer)
{
    auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
    xEventGroupSetBits(cloud_communication->wifi_eg, GET_ELEC_PRICE_EVENT_BIT);
}
void CloudCommunication::send_wifi_board_info_timer_cb(TimerHandle_t xTimer)
{
    auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
    EventBits_t wifi_bits = cloud_communication->ipstack->get_wifi_bits(pdMS_TO_TICKS(500));
    cloud_communication->send_wifi_status(wifi_bits & WIFI_CONNECTED_BIT);
    cloud_communication->send_hub_id();
}

void CloudCommunication::cloud_task(void *param)
{
    ESP_LOGI(TAG, "Cloud task started");

    auto cloud_communication = static_cast<CloudCommunication*>(param);
    auto ipstack = cloud_communication->ipstack;

    xTimerStart(cloud_communication->elec_price_req_timer_h, 0);
    xTimerStart(cloud_communication->send_wifi_board_info_timer_h, 0);

    std::vector<float> price_vec;
    while (true) {
        // ESP_LOGI(TAG, "Cloud task running");
        EventBits_t wifi_bits = ipstack->get_wifi_bits(portMAX_DELAY);

        if (wifi_bits & WIFI_FAIL_BIT) {
            xEventGroupClearBits(cloud_communication->wifi_eg, WIFI_FAIL_BIT);
            cloud_communication->send_wifi_status(false);
            esp_err_t err = ipstack->deinit_websocket();
            ESP_LOGI(TAG, "Websocket stoped: %s", esp_err_to_name(err));
        }

        if (wifi_bits & WIFI_CONNECTED_BIT) {    
            // ESP_LOGI(TAG, "Wifi connected");
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

        send_hub_id();
        // controller_data ctrl_data = {
        //     .device_id = 0,
        //     .type = DATA_TYPE_HUB_ID
        // };
        // snprintf(ctrl_data.data.c_value, sizeof(ctrl_data.data.c_value), "%s", efuse_mac);
        // if (xQueueSendToBack(controller_q, &ctrl_data, 0) == pdTRUE) {
        //     ESP_LOGI(TAG, "Hub id send to HubCommunication");
        // }
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
    if (xQueueReceive(cloud_q, &ctrl_data, pdMS_TO_TICKS(1000)) == pdTRUE
        && ctrl_data.type != DATA_TYPE_WIFI_SSID && ctrl_data.type != DATA_TYPE_WIFI_PW
    ) {
        // if (ctrl_data.type == DATA_TYPE_WIFI_SSID || ctrl_data.type == DATA_TYPE_WIFI_SSID) {
        //     return;
        // }
        // if (xQueueReceive(cloud_q, &ctrl_data, 0) != pdTRUE) {
        //     return;
        // }

        ESP_LOGI(TAG, "Sending data...");
        auto data_type_str = convertEnum<std::string_view>(ctrl_data.type);
        if (!data_type_str.has_value()) {
            ESP_LOGI(TAG, "Invalid datatype");
            return;
        }
        // std::string_view command_str = "UNKNOWN";
        // if (ctrl_data.type == DATA_TYPE_COMMAND) {
        //     command_str = convertEnum<std::string_view>(ctrl_data.data.command).value_or("UNKNOWN");
        // }

        std::ostringstream send_http_body_ss;
        send_http_body_ss << "device_id=" << ctrl_data.device_id
                    << "&type=" << data_type_str.value();

        if (ctrl_data.type == DATA_TYPE_POWER ||
            ctrl_data.type == DATA_TYPE_VOLTAGE ||
            ctrl_data.type == DATA_TYPE_CURRENT ||
            ctrl_data.type == DATA_TYPE_ENERGY ||
            ctrl_data.type == DATA_TYPE_THRESHOLD_MED ||
            ctrl_data.type == DATA_TYPE_THRESHOLD_LOW ||
            ctrl_data.type == DATA_TYPE_ELEC_PRICE
        ) {
            send_http_body_ss << "&value=" << ctrl_data.data.value;
        }
        else if (ctrl_data.type == DATA_TYPE_PRIORITY) {
            send_http_body_ss << "&value_int=" << ctrl_data.data.value_int;
        }
        // else if (ctrl_data.type == DATA_TYPE_COMMAND) {
        //     send_http_body_ss << "&command=" << ctrl_data.data.command;
        // }
        else if (ctrl_data.type == DATA_TYPE_SET_ON || ctrl_data.type == DATA_TYPE_ONLINE_STATE) {
            send_http_body_ss << "&flag=" << ctrl_data.data.flag;
        }
        else if (ctrl_data.type == DATA_TYPE_DEVICE_NAME) {
            send_http_body_ss << "&c_value=" << ctrl_data.data.c_value;
        }
        else if (ctrl_data.type != DATA_TYPE_DEVICE_LEFT) {
            ESP_LOGW(TAG, "send_data: Invalid data type");
            return;
        }
                    // << "&value=" << ctrl_data.data.value
                    // << "&value_int=" << ctrl_data.data.value_int
                    // << "&flag=" << ctrl_data.data.flag
                    // << "&command=" << command_str
                    // << "&c_value=" << ctrl_data.data.c_value;
        auto send_http_body = send_http_body_ss.str();
        ESP_LOGI(TAG, "%s", send_http_body.c_str());
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

            // Send to hub and cloud
            if (xQueueSendToBack(controller_q, &ctrl_data, 0) == pdTRUE
                && xQueueSendToBack(cloud_q, &ctrl_data, 0) == pdTRUE
            ) {
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

        // Send to hub and cloud
        if (xQueueSendToBack(controller_q, &ctrl_data, 0) == pdTRUE
            && xQueueSendToBack(cloud_q, &ctrl_data, 0) == pdTRUE
        ) {
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
        // ESP_LOGI(TAG, "Trying to parse websocket data");
        t_websocket_data ws_data = {};
        while (ipstack->get_websocket_data(&ws_data, pdMS_TO_TICKS(500))) {
            ESP_LOGI(TAG, "Websocket data: %s", ws_data.payload);
            std::string payload_str = ws_data.payload;
            auto parsed_cmd = split<std::string>(payload_str, '|');
            if (parsed_cmd.size() < 6) {
                ESP_LOGI(TAG, "No websocket data to process.");
                continue;
            }
        
            controller_data ctrl_data = {};

            // Parse controller_data.device_id
            auto device_id_str = parsed_cmd[0];
            uint64_t device_id = 0;
            auto [ptr, ec] = std::from_chars(device_id_str.data(),
                device_id_str.data() + device_id_str.size(), device_id);
        
            if (ec == std::errc{}) {
                ctrl_data.device_id = device_id;
            } else {
                ESP_LOGI(TAG, "Invalid device_id: %s", device_id_str.c_str());
                continue;
            }

            auto type = convertEnum<data_type_t>(parsed_cmd[1]);
            ctrl_data.type = type.value_or(DATA_TYPE_UNKNOWN);

            if (ctrl_data.type == DATA_TYPE_THRESHOLD_MED || ctrl_data.type == DATA_TYPE_THRESHOLD_LOW) {
                if (!try_string_to_float(parsed_cmd[2], ctrl_data.data.value)) {
                    ESP_LOGI(TAG, "Invalid value: %s", parsed_cmd[2].c_str());
                    continue;
                }
            }
            else if (ctrl_data.type == DATA_TYPE_PRIORITY) {
                if (!try_string_to_int(parsed_cmd[3], ctrl_data.data.value_int)) {
                    ESP_LOGI(TAG, "Invalid value_int: %s", parsed_cmd[3].c_str());
                    continue;
                }
            }
            else if (ctrl_data.type == DATA_TYPE_COMMAND) {
                auto command = convertEnum<commands>(parsed_cmd[4]);
                ctrl_data.data.command = command.value_or(UNKNOWN);
            }
            else if (ctrl_data.type == DATA_TYPE_DEVICE_NAME) {
                snprintf(ctrl_data.data.c_value, sizeof(ctrl_data.data.c_value),
                    "%s", parsed_cmd[5].c_str());
            } else {
                ESP_LOGI(TAG, "Unkown data type");
                continue;
            }
        
            if (xQueueSendToBack(controller_q, &ctrl_data, 0) != pdTRUE) {
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
    controller_data ctrl_data = {
        .device_id = 0,
        .type = DATA_TYPE_WIFI_ONLINE,
    };
    ctrl_data.data.flag = online;
    xQueueSendToBack(controller_q, &ctrl_data, 0);
}

void CloudCommunication::send_hub_id()
{
    controller_data ctrl_data = {
        .device_id = 0,
        .type = DATA_TYPE_HUB_ID
    };
    snprintf(ctrl_data.data.c_value, sizeof(ctrl_data.data.c_value), "%s", efuse_mac);
    xQueueSendToBack(controller_q, &ctrl_data, 0);
}
