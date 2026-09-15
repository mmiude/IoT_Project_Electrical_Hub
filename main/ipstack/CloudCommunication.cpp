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


CloudCommunication::CloudCommunication(IPStack *_ipstack,
    EventGroupHandle_t _wifi_eg, /*QueueHandle_t _tb_command_q, */QueueHandle_t _controller_q)
: ipstack(_ipstack), wifi_eg(_wifi_eg), /*tb_command_q(_tb_command_q), */controller_q(_controller_q)
{
    xTaskCreate(sign_task, "SIGN_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 2, NULL);

    xTaskCreate(tb_read_command_task, "TB_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 1, NULL);

    xTaskCreate(get_electricity_price_task, "EP_TASK", 3072, static_cast<void*>(this),
        tskIDLE_PRIORITY + 1, NULL);
}

void CloudCommunication::sign_task(void *param)
{
    ESP_LOGW(TAG, "starting sign_task.");
    auto cloud_communication = static_cast<CloudCommunication *>(param);
    auto ipstack = cloud_communication->ipstack;

    char *pcName = pcTaskGetName(NULL);

    if (ipstack->wait_for_wifi())
    {
        uint8_t mac[6];
        if (!get_efuse_mac(mac)) {
            ESP_LOGI(TAG, "%s: Error getting efuse mac", pcName);
            vTaskSuspend(NULL);
        }
        char formatted_mac[32] = {0};
        snprintf(formatted_mac, sizeof(formatted_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        char jwt_buffer[512] = {0};
        int jwt_error = generate_jwt(jwt_buffer, sizeof(jwt_buffer),
            DEVICE_JWT_SECRET, (const char *)formatted_mac);

        if (jwt_error < 0) {
            ESP_LOGI(TAG, "%s: Error generating JWT. Code: %d", pcName, jwt_error);
            vTaskSuspend(NULL);
        }

        std::map<std::string, std::string> auth_headers = {
            { "Authorization", std::string("Bearer ") + jwt_buffer }
        };
        // Allocate on heap instead of stack
        char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
        if (!buffer) {
            ESP_LOGE(TAG, "%s: Failed to allocate HTTP response buffer", pcName);
            vTaskDelete(NULL);
            return;
        }

        bool success = ipstack->http_request(API_HOSTNAME, API_PORT, buffer,
            "/api/initial_log_to_db", "", "",
            HTTP_METHOD_POST, auth_headers);

        // Memory cleanup
        free(buffer);
                
        if (success) {
            ESP_LOGI(TAG, "Go to: http://%s:%d/register_hub\nAnd enter code: %s\nTo register hub.", 
                API_HOSTNAME, API_PORT, formatted_mac);
        } else {
            ESP_LOGI(TAG, "%s: Error :(", pcName);
        }
    } 
    else {
        ESP_LOGI(TAG, "%s: Wifi not connected", pcName);
    }

    xEventGroupSetBits(cloud_communication->wifi_eg, DEVICE_SIGN_READY); 
    vTaskSuspend(NULL);
}

void CloudCommunication::tb_read_command_task(void *param)
{
    auto cloud_communication = static_cast<CloudCommunication *>(param);
    auto ipstack = cloud_communication->ipstack;

    char *pcName = pcTaskGetName(NULL);

    while (true) {
        if (ipstack->wait_for_wifi()) {
            ESP_LOGI(TAG, "%s: Fetching tb command...", pcName);
    
            int url_size = std::snprintf(nullptr, 0, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
            if (url_size <= 0) continue;
    
            std::string tb_url(url_size, '\0');
            std::snprintf(&tb_url[0], url_size + 1, THINGSPEACK_TB_URL, THINGSPEAK_TB_ID);
    
            std::ostringstream http_body_ss;
            http_body_ss << "api_key=" << THINGSPEAK_TB_API_KEY;
            auto http_body = http_body_ss.str();
    
            std::map<std::string, std::string> tb_headers = {
                { "Host", "api.thingspeak.com" },
                { "Content-Type", "application/x-www-form-urlencoded" },
                { "Accept", "*/*" }
            };
    
            char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
            if (!buffer) {
                ESP_LOGE(TAG, "%s: Failed to allocate HTTP response buffer", pcName);
                vTaskSuspend(NULL);
            }
            bool success = ipstack->http_request(tb_url.c_str(), buffer,
                http_body.c_str(), THINGSPEAK_CERT, HTTP_METHOD_POST, tb_headers);
    
            if (success) {
                controller_data ctrl_data = {};
                // if (cloud_communication->parse_talkback_response_json(buffer, &ctrl_data)) {
                if (parse_talkback_response_json(buffer, &ctrl_data)) {
                    ESP_LOGI(TAG, "%s: Succesfully parsed command\nCommand: %d\nDevice id: %" PRIu64,
                        pcName, static_cast<int>(ctrl_data.data.command), ctrl_data.device_id);
    
                    if (xQueueSendToBack(cloud_communication->controller_q, &ctrl_data, portMAX_DELAY) == pdTRUE) {
                        ESP_LOGI(TAG, "%s: Succesfully added command to queue", pcName);
                    } else {
                        ESP_LOGE(TAG, "%s: Error adding command to queue", pcName);
                    }
                } else {
                    ESP_LOGI(TAG, "%s: Error parsing command or no command in queue", pcName);
                }
            } else {
                ESP_LOGE(TAG, "%s: Error :(", pcName);
            }
            free(buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CloudCommunication::get_electricity_price_task(void *param)
{
    auto cloud_communication = static_cast<CloudCommunication *>(param);
    auto ipstack = cloud_communication->ipstack;

    char *pcName = pcTaskGetName(NULL);

    std::vector<float> price_vec;

    while (true) {
        if (price_vec.empty() && ipstack->wait_for_wifi()) {
            char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
            if (!buffer) {
                ESP_LOGE(TAG, "%s: Failed to allocate HTTP response buffer", pcName);
                vTaskSuspend(NULL);
            }
            bool success = ipstack->http_request(API_HOSTNAME, API_PORT,
                    buffer, "/api/get_electricity_prices");

            if (success) {
                std::string prices = buffer;
                price_vec = parseElectricityPrices(prices);
                ESP_LOGI(TAG, "%s: Got electricity prices for the next %d 15mins", pcName, price_vec.size());
            } else {
                ESP_LOGE(TAG, "%s: Error getting electricity prices", pcName);
            }
            free(buffer);
        }

        if (!price_vec.empty()) {
            float price = price_vec.back();
            controller_data ctrl_data = {};
            ctrl_data.type = DATA_TYPE_ELEC_PRICE;
            ctrl_data.data.value = price;

            if (xQueueSendToBack(cloud_communication->controller_q, &ctrl_data, portMAX_DELAY) == pdTRUE) {
                price_vec.pop_back();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(15 * MINUTE_TO_MS));
    }
}

// bool CloudCommunication::parse_talkback_response_json(const char *response, controller_data *ctrl_data)
// {
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
//                 auto parsed_cmd = split(json_val.substr(strlen(command)), '|');
//                 size_t cmd_size = parsed_cmd.size();

//                 if (cmd_size < 1) return false;

//                 auto command = stringToCommand(parsed_cmd[0]);
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