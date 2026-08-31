#include "device_sign.h"

static const char *TAG = "device_sign";

DeviceSign::DeviceSign(IPStack *_ipstack, EventGroupHandle_t _wifi_eg, TaskHandle_t _zbTaskHandle)
: ipstack(_ipstack), wifi_eg(_wifi_eg), zbTaskHandle(_zbTaskHandle)
{
    xTaskCreate(sign_task, "SIGN_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 1, NULL);
}

void DeviceSign::sign_task(void *param)
{
    auto device_sign = static_cast<DeviceSign*>(param);
    auto ipstack = device_sign->ipstack;

    EventBits_t bits = xEventGroupWaitBits(device_sign->wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        uint8_t mac[6];
        if (!get_efuse_mac(mac)) {
            ESP_LOGI(TAG, "Error getting efuse mac");
            vTaskSuspend(NULL);
        }
        char formatted_mac[32] = {0};
        snprintf(formatted_mac, sizeof(formatted_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        char jwt_buffer[512] = {0};
        int jwt_error = generate_jwt(jwt_buffer, sizeof(jwt_buffer),
            DEVICE_JWT_SECRET, (const char *)formatted_mac);

        if (jwt_error < 0) {
            ESP_LOGI(TAG, "Error generating JWT. Code: %d", jwt_error);
            vTaskSuspend(NULL);
        }

        std::map<std::string, std::string> auth_headers = {
            { "Authorization", std::string("Bearer ") + jwt_buffer }
        };
        // Allocate on heap instead of stack
        char *buffer = (char *)calloc(1, MAX_HTTP_OUTPUT_BUFFER + 1);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate HTTP response buffer");
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
            ESP_LOGI(TAG, "Error :(");
        }
    } 
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Wifi not connected");
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    xTaskNotifyGive(device_sign->zbTaskHandle);
    vTaskSuspend(NULL);
}