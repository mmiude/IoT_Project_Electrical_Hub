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
#include "device_sign.h"

#include "HubController.h"
#include "HubControllerEnums.h"


#define UART_PORT_NUM      UART_NUM_0
#define BUF_SIZE           (1024)

static const char *TAG = "MAIN"; 

typedef struct {
    QueueHandle_t q;
    EventGroupHandle_t events;
} dummy_task_params;

void dummy_task(void *params) {

    auto parameters = static_cast<dummy_task_params *> (params); 
    QueueSetHandle_t q = parameters->q;
    EventGroupHandle_t e = parameters->events;

    controller_data fake_low_threshold = {.type = DATA_TYPE_THRESHOLD_LOW};
    fake_low_threshold.data.threshold = 5.5;
    
    controller_data fake_med_threshold = {.type = DATA_TYPE_THRESHOLD_MED};
    fake_med_threshold.data.threshold = 7.5; 

    controller_data fake_electrical_price = {.type = DATA_TYPE_ELEC_PRICE};
    fake_electrical_price.data.electricity_price = 6.9; 

    xEventGroupWaitBits(e, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10000));
    xQueueSendToBack(q, &fake_low_threshold, 0);
    xQueueSendToBack(q, &fake_med_threshold, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xQueueSendToBack(q, &fake_electrical_price, 0);

    while (true) {
        fake_electrical_price.data.electricity_price = (rand() % 70) / 1.0; 
        vTaskDelay(pdMS_TO_TICKS(10000));
        xQueueSendToBack(q, &fake_electrical_price, 0);
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    EventGroupHandle_t wifi_eg = xEventGroupCreate();
    IPStack ipstack(SSID, PW, wifi_eg);

    DeviceSign device_sign(&ipstack, wifi_eg);
    static QueueHandle_t controllerQueue = xQueueCreate(5, sizeof(controller_data));

    static std::vector<std::shared_ptr<IDeviceProtocol>> protocols = {
        std::make_shared<ZigbeeCoordinator>(controllerQueue, wifi_eg)
    };

    static HubController controller(protocols, wifi_eg, controllerQueue);

    static dummy_task_params parameters = {.q = controllerQueue, .events = wifi_eg};

    xTaskCreate(dummy_task, "DUMMY", 1024, &parameters, tskIDLE_PRIORITY + 1, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}