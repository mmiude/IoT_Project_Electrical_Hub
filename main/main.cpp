#include <iostream>

#include "esp_log.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_gateway.h"
#include "ZigbeeCoordinator.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "network_info.h"
#include "IPStack.h"

#include "jwt.h"
#include "CloudCommunication.h"

#include "HubController.h"
#include "HubControllerEnums.h"


#define UART_PORT_NUM      UART_NUM_0
#define BUF_SIZE           (1024)

static const char *TAG = "MAIN"; 

typedef struct {
    QueueHandle_t q;
    EventGroupHandle_t events;
} dummy_task_params;

typedef struct {
    QueueHandle_t q_s;
    QueueHandle_t q_r;
    EventGroupHandle_t events;
} dummy_task_params_2;

void dummy_task(void *params) {

    auto parameters = static_cast<dummy_task_params *> (params); 
    QueueHandle_t q = parameters->q;
    EventGroupHandle_t e = parameters->events;

    controller_data fake_low_threshold = {.type = DATA_TYPE_THRESHOLD_LOW};
    fake_low_threshold.data.value = 5.5;
    
    controller_data fake_med_threshold = {.type = DATA_TYPE_THRESHOLD_MED};
    fake_med_threshold.data.value = 7.5; 

    controller_data fake_electrical_price = {.type = DATA_TYPE_ELEC_PRICE};
    fake_electrical_price.data.value = 6.9; 

    xEventGroupWaitBits(e, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10000));
    xQueueSendToBack(q, &fake_low_threshold, 0);
    xQueueSendToBack(q, &fake_med_threshold, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xQueueSendToBack(q, &fake_electrical_price, 0);

    while (true) {
        fake_electrical_price.data.value = (rand() % 10) / 1.0; 
        vTaskDelay(pdMS_TO_TICKS(30000));
        xQueueSendToBack(q, &fake_electrical_price, 0);
    }
}

void dummy_ui_task(void *params) {

    auto parameters = static_cast<dummy_task_params_2 *> (params);
    QueueHandle_t q_send = parameters->q_s;
    QueueHandle_t q_r = parameters->q_r;
    EventGroupHandle_t e = parameters->events;

    controller_data data_r; 
    controller_data data_s;

    xEventGroupWaitBits(e, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY);

    while (true) {
        if (xQueueReceive(q_r, &data_r, portMAX_DELAY) == pdPASS) {
            switch (data_r.type) {
                case DATA_TYPE_DEVICE_JOIN:
                    ESP_LOGW(TAG, "DUMMY UI: new device joind: 0x%016llx", data_r.device_id);
                    data_s = {.device_id = data_r.device_id, .type = DATA_TYPE_PRIORITY};
                    data_s.data.value_int = 1;
                    xQueueSendToBack(q_send, &data_s, 0);
                    break;
                case DATA_TYPE_DEVICE_LEFT:
                    ESP_LOGW(TAG, "DUMMY UI: device left notification");
                    break; 
                case DATA_TYPE_ONLINE_STATE:
                    ESP_LOGW(TAG, "DUMMY UI: dev: 0x%016llx: online state: %s", data_r.device_id, data_r.data.flag ? "ONLINE" : "OFFLINE");
                    break;
                case DATA_TYPE_POWER:
                    ESP_LOGW(TAG, "DUMMY UI: dev: 0x%016llx: power: %.4f", data_r.device_id, data_r.data.value);
                    break;
                case DATA_TYPE_ENERGY:
                    ESP_LOGW(TAG, "DUMMY UI: dev: 0x%016llx: energy: %.4f", data_r.device_id, data_r.data.value);
                    break; 
                case DATA_TYPE_SET_ON:
                    ESP_LOGW(TAG, "DUMMY UI: dev: 0x%016llx: set on: %s", data_r.device_id, data_r.data.flag ? "YES" : "NO");
                    break;
                case DATA_TYPE_SUPPORTS_METERING:
                    ESP_LOGW(TAG, "DUMMY UI: dev 0x%016llx: supports metering: %s", data_r.device_id, data_r.data.flag ? "YES" : "NO");
                    break;
                default:
                    ESP_LOGW(TAG, "DUMMY UI: unknown data type received"); 
                    break;
            }
        }
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    EventGroupHandle_t wifi_eg = xEventGroupCreate();
    IPStack ipstack(wifi_eg);
    ipstack.connect_wifi(SSID, PW);

    static QueueHandle_t controllerQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller receives all data from this queue. If task sends ANY data to controller it must be put here.
    static QueueHandle_t uiQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller sends data to local ui via this queue.
    static QueueHandle_t cloudQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller sends data to cloud via this queue.
    static QueueHandle_t tb_command_q = xQueueCreate(10, sizeof(HubCommand));

    CloudCommunication cloud_communication(&ipstack, wifi_eg, tb_command_q);

    static std::vector<std::shared_ptr<IDeviceProtocol>> protocols = {
        std::make_shared<ZigbeeCoordinator>(controllerQueue, wifi_eg)
    };

    static HubController controller(protocols, wifi_eg, controllerQueue, cloudQueue, uiQueue);

    static dummy_task_params parameters = {.q = controllerQueue, .events = wifi_eg};
    static dummy_task_params_2 params = {.q_s = controllerQueue, .q_r = uiQueue, .events = wifi_eg};

    xTaskCreate(dummy_task, "DUMMY", 1024, &parameters, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(dummy_ui_task, "DUMMY 2", 2048, &params, tskIDLE_PRIORITY + 1, NULL); 
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}