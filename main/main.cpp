#include "esp_log.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_gateway.h"


extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    ESP_LOGI(TAG, "Start ESP Zigbee Stack");
    xTaskCreate(esp_zigbee_stack_main_task, "Zigbee_main", 4096 * 2, NULL, 5, NULL);
}