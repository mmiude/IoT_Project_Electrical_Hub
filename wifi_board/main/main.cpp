
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
#include "HubEnums.h"
#include "Uart.h"
#include <memory>
#include "HubCommunicationManager.h"

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

    // ipstack.connect_wifi(SSID, PW);
    
    // static QueueHandle_t rx_queue = xQueueCreate(10, sizeof(controller_data));
    // static QueueHandle_t tx_queue = xQueueCreate(10, sizeof(controller_data));
    static QueueHandle_t controllerQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller receives all data from this queue. If task sends ANY data to controller it must be put here.
    static QueueHandle_t cloudQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller sends data to cloud via this queue - not yet implemented on controller side
    static QueueHandle_t wifiQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller sends data to local ui via this queue. Deeper than the others since the ui state sync replays every device at once.
    static QueueHandle_t uart_events;
    
    // static Uart uart(UART_NUM_1, GPIO_NUM_16, GPIO_NUM_17, rx_queue, tx_queue);
    static auto uart = std::make_shared<Uart>(UART_NUM_1, 16, 17, uart_events);
    IPStack ipstack(wifi_eg, wifiQueue);
    // static QueueHandle_t controllerQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller receives all data from this queue. If task sends ANY data to controller it must be put here.
    // static QueueHandle_t uiQueue = xQueueCreate(32, sizeof(controller_data)); // Hub controller sends data to local ui via this queue. Deeper than the others since the ui state sync replays every device at once.
    // static QueueHandle_t cloudQueue = xQueueCreate(10, sizeof(controller_data)); // Hub controller sends data to cloud via this queue - not yet implemented on controller side
    // static QueueHandle_t 


    static CloudCommunication cloud_communication(&ipstack, wifi_eg, cloudQueue, controllerQueue);
    static HubCommunicationManager cloud_comm(uart, cloudQueue, controllerQueue, wifiQueue, wifi_eg);
    // vTaskDelay(pdMS_TO_TICKS(5000));
    // controller_data ssid_data = {};
    // ssid_data.type = DATA_TYPE_WIFI_SSID;
    // snprintf(ssid_data.data.c_value, sizeof(ssid_data.data.c_value), "%s", SSID);
    // // snprintf(ssid_data.data.c_value, sizeof(ssid_data.data.c_value), "bs");

    // controller_data pw_data = {};
    // pw_data.type = DATA_TYPE_WIFI_PW;
    // snprintf(pw_data.data.c_value, sizeof(pw_data.data.c_value), "%s", PW);
    // // snprintf(pw_data.data.c_value, sizeof(pw_data.data.c_value), "bs");

    // vTaskDelay(pdMS_TO_TICKS(5000));
    // xQueueSendToBack(wifiQueue, &ssid_data, 0);
    // xQueueSendToBack(wifiQueue, &pw_data, 0);

    // uint64_t devId = 1234;
    // controller_data name_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_DEVICE_NAME
    // };
    // std::vector<controller_data> c_data_vev(12, data);
    // for (auto &d : c_data_vev) {

    // }

    // snprintf(name_data.data.c_value, sizeof(name_data.data.c_value), "test_device1234");
    // xQueueSendToBack(cloudQueue, &name_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data prio_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_PRIORITY
    // };
    // prio_data.data.value_int = 0;
    // xQueueSendToBack(cloudQueue, &prio_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // prio_data.data.value_int = 1;
    // xQueueSendToBack(cloudQueue, &prio_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // prio_data.data.value_int = 2;
    // xQueueSendToBack(cloudQueue, &prio_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));


    // controller_data set_on_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_SET_ON
    // };
    // set_on_data.data.flag = true;
    // xQueueSendToBack(cloudQueue, &set_on_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    
    // set_on_data.data.flag = false;
    // xQueueSendToBack(cloudQueue, &set_on_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data med_data = {
    //     .device_id = 0,
    //     .type = DATA_TYPE_THRESHOLD_MED
    // };
    // med_data.data.value = 12.3;
    // xQueueSendToBack(cloudQueue, &med_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data low_data = {
    //     .device_id = 0,
    //     .type = DATA_TYPE_THRESHOLD_LOW
    // };
    // low_data.data.value = 12.3;
    // xQueueSendToBack(cloudQueue, &low_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data power_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_POWER
    // };
    // power_data.data.value = 12.3;
    // xQueueSendToBack(cloudQueue, &power_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data energy_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_ENERGY
    // };
    // energy_data.data.value = 12.3;
    // xQueueSendToBack(cloudQueue, &energy_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data voltage_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_VOLTAGE
    // };
    // voltage_data.data.value = 12.3;
    // xQueueSendToBack(cloudQueue, &voltage_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // controller_data current_data = {
    //     .device_id = devId,
    //     .type = DATA_TYPE_CURRENT
    // };
    // current_data.data.value = 12.3;
    // xQueueSendToBack(cloudQueue, &current_data, 0);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    

    
    // xQueueSendToBack(wifiQueue, &ctrl_data, 0);
    // xQueueSendToBack(wifiQueue, &ctrl_data1, 0);
    // // ctrl_data.data.wifi_ssid = SSID;
    // xQueueSendToBack(rx_queue, &ctrl_data, 0);


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
