#include <iostream>

#include "esp_log.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_gateway.h"
#include "ZigbeeCoordinator.h"

void dummy_task(void *params) {
    static ZigbeeCoordinator coordinator;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        coordinator.electrical_values(); 
        coordinator.energy_consumption(); 
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    //static ZigbeeCoordinator coordinator;

    xTaskCreate(dummy_task, "DUMMY", 2048, NULL, tskIDLE_PRIORITY + 1, NULL); 
    
}