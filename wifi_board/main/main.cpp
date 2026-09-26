
#include "esp_log.h"
// #include "esp_zigbee.h"
// #include "ezbee/zha.h"
// #include "zigbee_gateway.h"
// #include "ZigbeeCoordinator.h"

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
#include "HubControllerEnums.h"
#include "uart.h"

static const char *TAG = "MAIN"; 


extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    // wifi pondering 
    // static auto sysConfStorage = std::make_shared<SystemConfigStorage>();
    // std::string saved_ssid, saved_pwd;
    // bool have_saved_wifi = sysConfStorage->get_wifi_info(saved_ssid, saved_pwd) == ESP_OK && !saved_ssid.empty();

    EventGroupHandle_t wifi_eg = xEventGroupCreate();
    // IPStack ipstack(wifi_eg);
    // if (have_saved_wifi) {
    //     ESP_LOGI(TAG, "connecting with saved wifi credentials (ssid: %s)", saved_ssid.c_str());
    //     ipstack.connect_wifi(saved_ssid.c_str(), saved_pwd.c_str());
    // } else {
    //     ESP_LOGI(TAG, "no saved wifi credentials, using network_info.h defaults");
    //    ipstack.connect_wifi(SSID, PW);
    // }

    // ipstack.connect_wifi(SSID, PW);

    IPStack ipstack(wifi_eg);
    ipstack.connect_wifi(SSID, PW);

    static QueueHandle_t rx_queue = xQueueCreate(10, sizeof(controller_data));
    static QueueHandle_t tx_queue = xQueueCreate(10, sizeof(controller_data));

    static Uart uart(UART_NUM_1, GPIO_NUM_16, GPIO_NUM_17, rx_queue, tx_queue);
    // static QueueHandle_t controllerQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller receives all data from this queue. If task sends ANY data to controller it must be put here.
    // static QueueHandle_t uiQueue = xQueueCreate(32, sizeof(controller_data)); // Hub controller sends data to local ui via this queue. Deeper than the others since the ui state sync replays every device at once.
    // static QueueHandle_t cloudQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller sends data to cloud via this queue - not yet implemented on controller side
    // static QueueHandle_t 


    static CloudCommunication cloud_communication(&ipstack, wifi_eg, rx_queue, tx_queue);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
