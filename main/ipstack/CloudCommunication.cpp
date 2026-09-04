#include "CloudCommunication.h"
#include <sstream>

static const char *TAG = "CloudCommunication";

CloudCommunication::CloudCommunication(IPStack *_ipstack,
    EventGroupHandle_t _wifi_eg, QueueHandle_t _tb_command_q)
: ipstack(_ipstack), wifi_eg(_wifi_eg), tb_command_q(_tb_command_q)
{
    xTaskCreate(sign_task, "SIGN_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 2, NULL);

    xTaskCreate(tb_read_command_task, "TB_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 1, NULL);
}

bool CloudCommunication::wait_for_wifi()
{
    EventBits_t bits = xEventGroupWaitBits(wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    return bits & WIFI_CONNECTED_BIT;
}

void CloudCommunication::sign_task(void *param)
{
    auto cloud_communication = static_cast<CloudCommunication *>(param);
    auto ipstack = cloud_communication->ipstack;

    char *pcName = pcTaskGetName(NULL);

    if (cloud_communication->wait_for_wifi())
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
            "/initial_log_to_db", "", "",
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

    while (cloud_communication->wait_for_wifi()) {
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
            continue;
        }
        bool success = ipstack->http_request(tb_url.c_str(), buffer,
            http_body.c_str(), THINGSPEAK_CERT, HTTP_METHOD_POST, tb_headers);

        if (success) {
            ESP_LOGI(TAG, "%s: Yaaay", pcName);
        } else {
            ESP_LOGE(TAG, "%s: Error :(", pcName);
        }
        free(buffer);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}