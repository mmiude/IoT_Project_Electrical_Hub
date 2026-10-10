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
// #include "IPStack.h"

// #include "jwt.h"
// #include "CloudCommunication.h"

#include "HubController.h"
#include "HubControllerEnums.h"

#include "ui_task.h"

#include "NvsStorage.h"
#include "DeviceInfoStorage.h"
#include "SystemConfigStorage.h"
#include "Led.h"
#include "SystemHealth.h"
#include "Uart.h"
#include "CloudCommunicationManager.h"
#include "FakerProtocol.h"


#define UART_PORT_NUM      UART_NUM_0
#define BUF_SIZE           (1024)

static const char *TAG = "MAIN"; 


extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME));

    //EventGroupHandle_t wifi_eg = xEventGroupCreate();
    EventGroupHandle_t sys_event_bits = xEventGroupCreate(); 
    //IPStack ipstack(wifi_eg);
    // if (have_saved_wifi) {
    //     ESP_LOGI(TAG, "connecting with saved wifi credentials (ssid: %s)", saved_ssid.c_str());
    //     ipstack.connect_wifi(saved_ssid.c_str(), saved_pwd.c_str());
    // } else {
    //     ESP_LOGI(TAG, "no saved wifi credentials, using network_info.h defaults");
    //    ipstack.connect_wifi(SSID, PW);
    // }

    //ipstack.connect_wifi(SSID, PW);

    static QueueHandle_t controllerQueue = xQueueCreate(25, sizeof(controller_data)); // Hub controller receives all data from this queue. If task sends ANY data to controller it must be put here.
    static QueueHandle_t uiQueue = xQueueCreate(25, sizeof(controller_data)); // Hub controller sends data to local ui via this queue. Deeper than the others since the ui state sync replays every device at once.
    static QueueHandle_t cloudQueue = xQueueCreate(25, sizeof(controller_data)); // Hub controller sends data to cloud via this queue - not yet implemented on controller side
    static QueueHandle_t uart_events;
    //CloudCommunication cloud_communication(&ipstack, wifi_eg, cloudQueue, controllerQueue);

    static auto coordinatorStorage = std::make_shared<DeviceInfoStorage<smartPlugInfo>>("zb_ns", "zb_dev_info");
    static auto controllerStorage = std::make_shared<DeviceInfoStorage<deviceInfo>>("ctrl_ns", "ctrl_dev_info");
    static auto uiStorage = std::make_shared<DeviceInfoStorage<UiDeviceRecord>>("ui_ns", "ui_dev_info");
    static auto fakerStorage = std::make_shared<DeviceInfoStorage<f_dev>>("f_ns", "f_dev_info");
    static auto sysConfStorage = std::make_shared<SystemConfigStorage>(); // still needed for thresholds; wifi saving is disabled above
    

    static auto leds = std::make_shared<Led>(GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_3); 

    //coordinatorStorage->erase_name_space();
    //controllerStorage->erase_name_space();
    //sysConfStorage->erase_all_system_config_info();
    //uiStorage->erase_name_space();
    //fakerStorage->erase_name_space();

    static std::vector<std::shared_ptr<IDeviceProtocol>> protocols = {
        std::make_shared<ZigbeeCoordinator>(controllerQueue, sys_event_bits, coordinatorStorage),
        std::make_shared<FakerProtocol>(controllerQueue, sys_event_bits, fakerStorage)
    };

    static HubController controller(protocols, sys_event_bits, controllerQueue, cloudQueue, uiQueue, controllerStorage, sysConfStorage);
    controller.attach(leds);

    static SystemHealth systemHealthMonitor(sys_event_bits, controllerQueue); 

    static UiTask ui(controllerQueue, cloudQueue, uiQueue, sys_event_bits, uiStorage);

    static auto uart = std::make_shared<Uart>(UART_NUM_1, 16, 17, uart_events);
    static CloudCommunicationManager cloud_comm(uart, sys_event_bits, controllerQueue, cloudQueue, uiQueue);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}