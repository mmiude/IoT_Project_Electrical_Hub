#include "CloudCommunication.h"
#include <sstream>
#include <unordered_map>
#include <optional>
#include <cstdlib>
#include <cerrno>
#include <charconv>
#include <string_view>
#include <memory>
// #include "zigbee_gateway.h"

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

// static bool parse_talkback_response_json(const char *response, controller_data *ctrl_data) {
//     if (!response || !ctrl_data) return false;

//     jsmn_parser parser;
//     jsmn_init(&parser);

//     std::string response_str = response;

//     std::string json = "";
//     size_t json_start = response_str.find("{");
//     size_t json_end = response_str.rfind("}");
//     if (json_start == std::string::npos || json_end == std::string::npos) return false;

//     json = response_str.substr(json_start, json_end - json_start + 1);
//     jsmntok_t tokens[JSMN_TOKENS_SIZE];
//     int r = jsmn_parse(&parser, json.c_str(), json.size(), tokens, JSMN_TOKENS_SIZE);
//     if (r < 0) return false;

//     const char command[] = "HUB_COMMAND|";

//     for (int i = 0; i < JSMN_TOKENS_SIZE; i++) {
//         if (tokens[i].type == JSMN_STRING) {
//             std::string json_val = json.substr(tokens[i].start, tokens[i].end - tokens[i].start);
//             if (json_val.find(command) != std::string::npos) {
//                 auto parsed_cmd = split<std::string>(json_val.substr(strlen(command)), '|');
//                 size_t cmd_size = parsed_cmd.size();

//                 if (cmd_size < 1) return false;

//                 // auto command = stringToCommand(parsed_cmd[0]);
//                 // std::string str_command = parsed_cmd[0];
//                 auto command = convertEnum<commands>(parsed_cmd[0]);
//                 if (command.has_value()) {
//                     ctrl_data->data.command = command.value();
//                 } else {
//                     return false;
//                 }

//                 if (cmd_size < 2) return true;

//                 auto device_id_str = parsed_cmd[1];
//                 uint64_t device_id = 0;
//                 auto [ptr, ec] = std::from_chars(device_id_str.data(),
//                     device_id_str.data() + device_id_str.size(), device_id);

//                 if (ec == std::errc{}) {
//                     ctrl_data->device_id = device_id;
//                     return true;
//                 }
//             }
//         }
//     }
//     return false;
// }


CloudCommunication::CloudCommunication(IPStack *_ipstack, EventGroupHandle_t _wifi_eg,
    QueueHandle_t _rx_queue, QueueHandle_t _tx_queue)
: ipstack(_ipstack), wifi_eg(_wifi_eg), rx_queue(_rx_queue), tx_queue(_tx_queue)
{
    // cloud_control_q = xQueueCreate(5, sizeof(int));

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

    // int url_size = std::snprintf(nullptr, 0, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
    // if (url_size > 0) {
    //     tb_url.resize(static_cast<size_t>(url_size));
    //     std::snprintf(tb_url.data(), tb_url.size() + 1, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
    // }

    // std::ostringstream read_http_body_ss;
    // read_http_body_ss << "api_key=" << THINGSPEAK_TB_API_KEY;
    // read_http_body = read_http_body_ss.str();

    std::ostringstream ws_url_ss;
    ws_url_ss << "ws://" << API_HOSTNAME
            << ":" << WS_PORT
            << "/ws?hub=" << efuse_mac;
    ws_url = ws_url_ss.str();

    elec_price_req_timer_h = xTimerCreate("ELEC_PRICE_REQ", pdMS_TO_TICKS(15 * MINUTE_TO_MS), pdTRUE,
        static_cast<void*>(this), elec_price_req_timer_cb);
    // cloud_comm_timer_h = xTimerCreate("CLOUD_COMM", pdMS_TO_TICKS(5000), pdTRUE,
    //     static_cast<void*>(this), send_data_timer_cb);

    xTaskCreate(cloud_task, "CLOUD_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 2, &cloud_task_handle);
}

CloudCommunication::~CloudCommunication() {
    // if (cloud_comm_timer_h) xTimerDelete(cloud_comm_timer_h, portMAX_DELAY);
    if (elec_price_req_timer_h) xTimerDelete(elec_price_req_timer_h, portMAX_DELAY);
    // if (cloud_control_q) vQueueDelete(cloud_control_q);
    if (cloud_task_handle) vTaskDelete(cloud_task_handle);
}

void CloudCommunication::elec_price_req_timer_cb(TimerHandle_t xTimer)
{
    auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
    xEventGroupSetBits(cloud_communication->wifi_eg, GET_ELEC_PRICE_EVENT_BIT);
    // int i = 0;
    // xQueueSendToBack(cloud_communication->cloud_control_q, &i, 0);
}
// void CloudCommunication::send_data_timer_cb(TimerHandle_t xTimer)
// {
//     auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
//     xEventGroupSetBits(cloud_communication->wifi_eg, SEND_DATA_EVENT_BIT);
//     // int i = 1;
//     // xQueueSendToBack(cloud_communication->cloud_control_q, &i, 0);
// }

void CloudCommunication::cloud_task(void *param)
{
    ESP_LOGI(TAG, "Cloud task started");

    auto cloud_communication = static_cast<CloudCommunication*>(param);
    auto ipstack = cloud_communication->ipstack;

    // xEventGroupWaitBits(cloud_communication->wifi_eg,
    //     ZIGBEE_STACK_READY,
    //     pdFALSE,
    //     pdFALSE,
    //     portMAX_DELAY
    // );
    // ESP_LOGI(TAG, "Zigbee ready starting cloud task");

    // if (ipstack)
    // cloud_communication->validate_hub();

    // xTimerStart(cloud_communication->cloud_comm_timer_h, 0);
    xTimerStart(cloud_communication->elec_price_req_timer_h, 0);

    std::vector<float> price_vec;
    // cloud_communication->get_electricity_price(price_vec);

    // bool sending = false;
    int action;
    while (true) {
        // vTaskDelay(pdMS_TO_TICKS(500));
        if (!cloud_communication->ipstack->wait_for_wifi()) {
            ESP_LOGE(TAG, "No wifi");
            continue;
        }
        // sending = !sending;

        // EventBits_t bits = xEventGroupGetBits(cloud_communication->wifi_eg);
        EventBits_t bits = xEventGroupWaitBits(cloud_communication->wifi_eg,
            ON_WIFI_CONNECT_BIT | GET_ELEC_PRICE_EVENT_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(50)
        );
        if (bits & ON_WIFI_CONNECT_BIT) {
            ESP_LOGI(TAG, "Wifi connection detected.");
            cloud_communication->validate_hub();
            cloud_communication->get_electricity_price(price_vec);
            cloud_communication->connect_websocket();
            // xEventGroupClearBits(cloud_communication->wifi_eg, ON_WIFI_CONNECT_BIT);
            // vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (bits & GET_ELEC_PRICE_EVENT_BIT) {
            cloud_communication->get_electricity_price(price_vec);
        }

        // if (xQueueReceive(cloud_communication->rx_queue))
        cloud_communication->send_data();
        cloud_communication->parse_websocket_data();


        // if (comm_bits & SEND_DATA_EVENT_BIT) {
        //     cloud_communication->send_data();
        // }
        // if (comm_bits & GET_ELEC_PRICE_EVENT_BIT) {
        //     cloud_communication->get_electricity_price(price_vec);
        // }

        // bool success = xQueueReceive(cloud_communication->cloud_control_q, &action, pdMS_TO_TICKS(100)) == pdTRUE;
        // if (success && action == 1) {
        //     cloud_communication->send_data();
        // }
        // // else if (success && action == 1) {
        // //     // cloud_communication->read_data();
        // //     cloud_communication->parse_websocket_data();
        // // }
        // else if (success && action == 0) {
        //     cloud_communication->get_electricity_price(price_vec);
        // }
        // vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void CloudCommunication::validate_hub()
{
    if (auth_headers.empty()) {
        ESP_LOGI(TAG, "No auth headers found");
        return;
    }

        // std::map<std::string, std::string> auth_headers = {
        //     { "Authorization", std::string("Bearer ") + cloud_communication->hub_jwt }
        // };
        // Allocate on heap instead of stack
    // char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
    auto buffer = std::unique_ptr<char, decltype(&std::free)>(
        static_cast<char*>(calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1)), 
        std::free
    );
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
        // xSemaphoreGive(cloud_communication->ipstack_mtx);
        // vTaskDelete(NULL);
        return;
    }

    bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer.get(),
        "/api/initial_log_to_db", "", "",
        HTTP_METHOD_POST, auth_headers);

    // Memory cleanup
    // free(buffer);

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
    // std::ostringstream send_http_body_ss;
    // send_http_body_ss << "{ "

    // while (xQueueReceive(cloud_q, &ctrl_data, 0) == pdTRUE) {

    // }

    // TODO: recieve data from UART
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

        // char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
        auto buffer = std::unique_ptr<char, decltype(&std::free)>(
            static_cast<char*>(calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1)), 
            std::free
        );
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
            // xSemaphoreGive(cloud_communication->ipstack_mtx);
            // vTaskDelete(NULL);
            return;
        }
        auto headers = auth_headers;
        headers.insert({ "Content-Type", "application/x-www-form-urlencoded" });

        bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer.get(),
            "/api/send_device_data", "", send_http_body.c_str(), HTTP_METHOD_POST, headers);
        // free(buffer);

        ESP_LOGI(TAG, "Data send %s", success ? "successull" : "failed");
    }
}

// void CloudCommunication::read_data()
// {
//     if (tb_headers.empty() || tb_url.empty() || read_http_body.empty()) {
//         ESP_LOGI(TAG, "Crucial parameters not found.");
//         return;
//     }

// void CloudCommunication::read_data()
// {
//     if (tb_headers.empty() || tb_url.empty() || read_http_body.empty()) {
//         ESP_LOGI(TAG, "Crucial parameters not found.");
//         return;
//     }

//     ESP_LOGI(TAG, "Fetching tb command...");

//     char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
//     if (!buffer) {
//         ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
//         // xSemaphoreGive(cloud_communication->ipstack_mtx);
//         // vTaskSuspend(NULL);
//         return;
//     }
//     bool success = ipstack->http_request(tb_url.c_str(), buffer,
//         read_http_body.c_str(), THINGSPEAK_CERT, HTTP_METHOD_POST, tb_headers);

//     controller_data ctrl_data = {};
//     bool parsed = parse_talkback_response_json(buffer, &ctrl_data);
//     free(buffer);

//     if (success && parsed && xQueueSendToBack(controller_q, &ctrl_data, 0) == pdTRUE) {
//         ESP_LOGI(TAG, "Added command to queue\nCommand: %d\nDevice id: %" PRIu64,
//                 static_cast<int>(ctrl_data.data.command), ctrl_data.device_id);
//     } else if (success && parsed) {
//         ESP_LOGE(TAG, "Error adding command to queue");
//     } else if (success) {
//         ESP_LOGI(TAG, "Error parsing command or no command in queue.");
//     } else {
//         ESP_LOGE(TAG, "HTTP error.");
//     }
// }
//     ESP_LOGI(TAG, "Fetching tb command...");

//     char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
//     if (!buffer) {
//         ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
//         // xSemaphoreGive(cloud_communication->ipstack_mtx);
//         // vTaskSuspend(NULL);
//         return;
//     }
//     bool success = ipstack->http_request(tb_url.c_str(), buffer,
//         read_http_body.c_str(), THINGSPEAK_CERT, HTTP_METHOD_POST, tb_headers);

//     controller_data ctrl_data = {};
//     bool parsed = parse_talkback_response_json(buffer, &ctrl_data);
//     free(buffer);

//     if (success && parsed && xQueueSendToBack(controller_q, &ctrl_data, 0) == pdTRUE) {
//         ESP_LOGI(TAG, "Added command to queue\nCommand: %d\nDevice id: %" PRIu64,
//                 static_cast<int>(ctrl_data.data.command), ctrl_data.device_id);
//     } else if (success && parsed) {
//         ESP_LOGE(TAG, "Error adding command to queue");
//     } else if (success) {
//         ESP_LOGI(TAG, "Error parsing command or no command in queue.");
//     } else {
//         ESP_LOGE(TAG, "HTTP error.");
//     }
// }

void CloudCommunication::get_electricity_price(std::vector<float> &price_vec)
{
    if (price_vec.empty()) {
        auto buffer = std::unique_ptr<char, decltype(&std::free)>(
            static_cast<char*>(calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1)), 
            std::free
        );
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
            // xSemaphoreGive(cloud_communication->ipstack_mtx);
            // vTaskSuspend(NULL);
            return;
        }
        bool success = ipstack->http_request(API_HOSTNAME, API_PORT,
                buffer.get(), "/api/get_electricity_prices");

        if (success) {
            std::string prices = buffer.get();
            // price_vec = parseElectricityPrices(prices);
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
        // xSemaphoreGive(cloud_communication->ipstack_mtx);
        // free(buffer);
    } else {
        controller_data ctrl_data = {};
        ctrl_data.type = DATA_TYPE_ELEC_PRICE;
        ctrl_data.data.value = price_vec.back();

        // TODO: pass data to UART
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
    // if (!ipstack->init_websocket(ws_url.c_str())) {
    //     ESP_LOGE(TAG, )
    // }
}

void CloudCommunication::parse_websocket_data()
{
    // ESP_LOGI(TAG, "Trying to get websocket data...");
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
        
            // TODO: Pass data to UART
            if (xQueueSendToBack(tx_queue, &ctrl_data, 0) != pdTRUE) {
                ESP_LOGI(TAG, "Failed to send data to controller queue");
                continue;
            }
            ESP_LOGI(TAG, "Websocket data processed succesfully :)");
    
        }
        return;
    }
    // ESP_LOGI(TAG, "Websocket not connected");
    // if (!ipstack->get_websocket_data(&ws_data, 0)) {
    //     ESP_LOGI(TAG, "No websocket data to process.");
    //     return;
    // }

}
