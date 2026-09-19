#include "CloudCommunication.h"
#include <sstream>
#include <unordered_map>
#include <optional>
#include <cstdlib>
#include <cerrno>
#include <charconv>

static const char *TAG = "CloudCommunication";

static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(str);

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

static std::optional<commands> stringToCommand(const std::string& str) {
    static const std::unordered_map<std::string, commands> commandMap = {
        { "TOGGLE_PLUG", commands::TOGGLE_PLUG },
        { "PLUG_ON", commands::PLUG_ON },
        { "PLUG_OFF", commands::PLUG_OFF },
        { "OPEN_NETWORK", commands::OPEN_NETWORK }
    };

    auto it = commandMap.find(str);
    if (it != commandMap.end()) {
        return it->second;
    }
    return std::nullopt;
}

static std::vector<float> parseElectricityPrices(const std::string& input) {
    std::vector<float> result;
    std::stringstream ss(input);
    std::string token;

    while (std::getline(ss, token, ',')) {
        result.push_back(std::stof(token));
    }

    return result;
}

static bool parse_talkback_response_json(const char *response, controller_data *ctrl_data) {
    if (!response || !ctrl_data) return false;

    jsmn_parser parser;
    jsmn_init(&parser);

    std::string response_str = response;

    std::string json = "";
    size_t json_start = response_str.find("{"); 
    size_t json_end = response_str.rfind("}");
    if (json_start == std::string::npos || json_end == std::string::npos) return false;

    json = response_str.substr(json_start, json_end - json_start + 1);
    jsmntok_t tokens[JSMN_TOKENS_SIZE];
    int r = jsmn_parse(&parser, json.c_str(), json.size(), tokens, JSMN_TOKENS_SIZE);
    if (r < 0) return false;

    const char command[] = "HUB_COMMAND|";

    for (int i = 0; i < JSMN_TOKENS_SIZE; i++) {
        if (tokens[i].type == JSMN_STRING) {
            std::string json_val = json.substr(tokens[i].start, tokens[i].end - tokens[i].start);
            if (json_val.find(command) != std::string::npos) {
                auto parsed_cmd = split(json_val.substr(strlen(command)), '|');
                size_t cmd_size = parsed_cmd.size();

                if (cmd_size < 1) return false;

                auto command = stringToCommand(parsed_cmd[0]);
                if (command.has_value()) {
                    ctrl_data->data.command = command.value();
                } else {
                    return false;
                }

                if (cmd_size < 2) return true;

                auto device_id_str = parsed_cmd[1];
                uint64_t device_id = 0;
                auto [ptr, ec] = std::from_chars(device_id_str.data(),
                    device_id_str.data() + device_id_str.size(), device_id);
                
                if (ec == std::errc{}) {
                    ctrl_data->device_id = device_id;
                    return true;
                }
            }
        }
    }
    return false;
}


CloudCommunication::CloudCommunication(IPStack *_ipstack, EventGroupHandle_t _wifi_eg,
    QueueHandle_t _cloud_q, QueueHandle_t _controller_q)
: ipstack(_ipstack), wifi_eg(_wifi_eg), cloud_q(_cloud_q), controller_q(_controller_q)
{
    cloud_control_q = xQueueCreate(5, sizeof(int));

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

    int url_size = std::snprintf(nullptr, 0, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
    if (url_size > 0) {
        tb_url.resize(static_cast<size_t>(url_size));
        std::snprintf(tb_url.data(), tb_url.size() + 1, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
    }

    std::ostringstream read_http_body_ss;
    read_http_body_ss << "api_key=" << THINGSPEAK_TB_API_KEY;
    read_http_body = read_http_body_ss.str();

    elec_price_req_timer_h = xTimerCreate("ELEC_PRICE_REQ", pdMS_TO_TICKS(15 * MINUTE_TO_MS), pdTRUE,
        static_cast<void*>(this), elec_price_req_timer_cb);
    cloud_comm_timer_h = xTimerCreate("CLOUD_COMM", pdMS_TO_TICKS(5000), pdTRUE,
        static_cast<void*>(this), cloud_comm_timer_cb);

    
    xTaskCreate(cloud_task, "CLOUD_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 1, NULL);


    // xTaskCreate(sign_task, "SIGN_TASK", 4096, static_cast<void*>(this),
    //     tskIDLE_PRIORITY + 2, NULL);

    // xTaskCreate(read_and_send_task, "R_AND_S_TASK", 4096, static_cast<void*>(this),
    //     tskIDLE_PRIORITY + 1, NULL);

    // xTaskCreate(get_electricity_price_task, "EP_TASK", 3072, static_cast<void*>(this),
    //     tskIDLE_PRIORITY + 1, NULL);
}

void CloudCommunication::elec_price_req_timer_cb(TimerHandle_t xTimer)
{
    auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
    int i = 0;
    xQueueSendToBack(cloud_communication->cloud_control_q, &i, 0);
}
void CloudCommunication::cloud_comm_timer_cb(TimerHandle_t xTimer)
{
    auto cloud_communication = static_cast<CloudCommunication*>(pvTimerGetTimerID(xTimer));
    int i = 1;
    xQueueSendToBack(cloud_communication->cloud_control_q, &i, 0);
}

void CloudCommunication::cloud_task(void *param)
{
    ESP_LOGI(TAG, "Cloud task started");

    auto cloud_communication = static_cast<CloudCommunication*>(param);

    // if (ipstack)
    // cloud_communication->validate_device();

    xTimerStart(cloud_communication->cloud_comm_timer_h, 0);
    xTimerStart(cloud_communication->elec_price_req_timer_h, 0);

    std::vector<float> price_vec;
    // cloud_communication->get_electricity_price(price_vec);

    bool sending = false;
    int action;
    while (true) {
        if (!cloud_communication->ipstack->wait_for_wifi()) {
            ESP_LOGE(TAG, "No wifi");
            continue;
        }
        sending = !sending;

        EventBits_t bits = xEventGroupGetBits(cloud_communication->wifi_eg);
        if (bits & ON_WIFI_CONNECT_BIT) {
            ESP_LOGI(TAG, "Wifi connection detected.");
            cloud_communication->validate_device();
            cloud_communication->get_electricity_price(price_vec);
            xEventGroupClearBits(cloud_communication->wifi_eg, ON_WIFI_CONNECT_BIT);
        }

        bool success = xQueueReceive(cloud_communication->cloud_control_q, &action, portMAX_DELAY) == pdTRUE;
        if (success && action == 1 && sending) {
            cloud_communication->send_data();
        }
        else if (success && action == 1) {
            cloud_communication->read_data();
        }
        else if (success && action == 0) {
            cloud_communication->get_electricity_price(price_vec);
        }
    }
}

void CloudCommunication::validate_device()
{
    if (auth_headers.empty()) {
        ESP_LOGI(TAG, "No auth headers found");
        return;
    }

        // std::map<std::string, std::string> auth_headers = {
        //     { "Authorization", std::string("Bearer ") + cloud_communication->hub_jwt }
        // };
        // Allocate on heap instead of stack
    char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
        // xSemaphoreGive(cloud_communication->ipstack_mtx);
        // vTaskDelete(NULL);
        return;
    }

    bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer,
        "/api/initial_log_to_db", "", "",
        HTTP_METHOD_POST, auth_headers);

    // Memory cleanup
    free(buffer);
            
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

    ESP_LOGI(TAG, "Sending data...");
    controller_data ctrl_data;
    if (xQueueReceive(cloud_q, &ctrl_data, pdMS_TO_TICKS(500)) == pdTRUE) {
        std::ostringstream send_http_body_ss;
        send_http_body_ss << "device_id=" << ctrl_data.device_id
                    << "&type=" << ctrl_data.type
                    << "&value=" << ctrl_data.data.value
                    << "&value_int=" << ctrl_data.data.value_int;
        auto send_http_body = send_http_body_ss.str();
        // ESP_LOGI(TAG, "%s: %s", pcName, send_http_body.c_str());

        char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
            // xSemaphoreGive(cloud_communication->ipstack_mtx);
            // vTaskDelete(NULL);
            return;
        }
        auto headers = auth_headers;
        headers.insert({ "Content-Type", "application/x-www-form-urlencoded" });

        bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer,
            "/api/send_device_data", "", send_http_body.c_str(), HTTP_METHOD_POST, headers);
        free(buffer);

        ESP_LOGI(TAG, "Data send %s", success ? "successull" : "failed");
    } else {
        ESP_LOGI(TAG, "No data to send.");
    }
}

void CloudCommunication::read_data()
{
    if (tb_headers.empty() || tb_url.empty() || read_http_body.empty()) {
        ESP_LOGI(TAG, "Crucial parameters not found.");
        return;
    }

    ESP_LOGI(TAG, "Fetching tb command...");

// int url_size = std::snprintf(nullptr, 0, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
// if (url_size <= 0) {
//     // sending = true;
//     xSemaphoreGive(cloud_communication->ipstack_mtx);
//     continue;
// }

// std::string tb_url(url_size, '\0');
// std::snprintf(&tb_url[0], url_size + 1, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);

// std::ostringstream http_body_ss;
// http_body_ss << "api_key=" << THINGSPEAK_TB_API_KEY;
// auto http_body = http_body_ss.str();

// std::map<std::string, std::string> tb_headers = {
//     { "Host", "api.thingspeak.com" },
//     { "Content-Type", "application/x-www-form-urlencoded" },
//     { "Accept", "*/*" }
// };

    char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
        // xSemaphoreGive(cloud_communication->ipstack_mtx);
        // vTaskSuspend(NULL);
        return;
    }
    bool success = ipstack->http_request(tb_url.c_str(), buffer,
        read_http_body.c_str(), THINGSPEAK_CERT, HTTP_METHOD_POST, tb_headers);

    controller_data ctrl_data = {};
    bool parsed = parse_talkback_response_json(buffer, &ctrl_data);
    free(buffer);

    if (success && parsed && xQueueSendToBack(controller_q, &ctrl_data, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Added command to queue\nCommand: %d\nDevice id: %" PRIu64,
                static_cast<int>(ctrl_data.data.command), ctrl_data.device_id);
    } else if (success && parsed) {
        ESP_LOGE(TAG, "Error adding command to queue");
    } else if (success) {
        ESP_LOGI(TAG, "Error parsing command or no command in queue.");
    } else {
        ESP_LOGE(TAG, "HTTP error.");
    }
}

void CloudCommunication::get_electricity_price(std::vector<float> &price_vec)
{
    if (price_vec.empty()) {
        char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
            // xSemaphoreGive(cloud_communication->ipstack_mtx);
            // vTaskSuspend(NULL);
            return;
        }
        bool success = ipstack->http_request(API_HOSTNAME, API_PORT,
                buffer, "/api/get_electricity_prices");

        if (success) {
            std::string prices = buffer;
            price_vec = parseElectricityPrices(prices);

            ESP_LOGI(TAG, "Got electricity prices for the next %d 15mins", price_vec.size());

            controller_data ctrl_data = {};
            ctrl_data.type = DATA_TYPE_ELEC_PRICE;
            ctrl_data.data.value = price_vec.back();

            if (xQueueSendToBack(controller_q, &ctrl_data, portMAX_DELAY) == pdTRUE) {
                ESP_LOGI(TAG, "Electricity price updated to: %.2f", ctrl_data.data.value);
                price_vec.pop_back();
            }
        } else {
            ESP_LOGE(TAG, "Error getting electricity prices");
        }
        // xSemaphoreGive(cloud_communication->ipstack_mtx);
        free(buffer);
    } else {
        controller_data ctrl_data = {};
        ctrl_data.type = DATA_TYPE_ELEC_PRICE;
        ctrl_data.data.value = price_vec.back();

        if (xQueueSendToBack(controller_q, &ctrl_data, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Electricity price updated to: %.2f", ctrl_data.data.value);
            price_vec.pop_back();
        }
    }
}
