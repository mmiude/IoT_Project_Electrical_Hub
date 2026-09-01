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


#define UART_PORT_NUM      UART_NUM_0
#define BUF_SIZE           (1024)

static const char *TAG = "MAIN"; 


void dummy_task(void *params) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    static ZigbeeCoordinator coordinator;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
        coordinator.get_electrical_values(1, 1); 
        coordinator.get_energy_consumption(1, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "DEVICE COUNT: %d", coordinator.check_device_count());
        vTaskDelay(pdMS_TO_TICKS(5000));
        coordinator.set_smart_plug_on(1, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        coordinator.get_on_off_state(1, 1); 
        vTaskDelay(pdMS_TO_TICKS(5000));
        coordinator.open_network();
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    EventGroupHandle_t wifi_eg = xEventGroupCreate();
    IPStack ipstack(SSID, PW, wifi_eg);

    static TaskHandle_t dummy_task_handle;

    xTaskCreate(dummy_task, "DUMMY", 2048, &wifi_eg, tskIDLE_PRIORITY + 1, &dummy_task_handle); 
    DeviceSign device_sign(&ipstack, wifi_eg, dummy_task_handle);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}