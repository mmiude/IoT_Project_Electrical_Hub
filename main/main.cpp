#include <iostream>

#include "esp_log.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_gateway.h"
#include "ZigbeeCoordinator.h"

static const char *TAG = "MAIN"; 

void dummy_task(void *params) {
    static ZigbeeCoordinator coordinator;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        coordinator.get_electrical_values(1); 
        coordinator.get_energy_consumption(1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "DEVICE COUNT: %d", coordinator.check_device_count()); 
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    //static ZigbeeCoordinator coordinator;

    xTaskCreate(dummy_task, "DUMMY", 2048, NULL, tskIDLE_PRIORITY + 1, NULL); 
    
}