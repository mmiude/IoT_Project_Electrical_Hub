#include "HubController.h"

//#include "zigbee_gateway.h"

static const char *TAG = "HUB_CONTROLLER"; 


HubController::HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events, QueueHandle_t controller_q) : plugProtocols(protocols), event_group(events), controller_queue(controller_q){
    xTaskCreate(HubController::runner, "HUB_CONTROLLER", 2048, this, tskIDLE_PRIORITY + 1, &handle);
}

void HubController::runner(void *params){
    auto instance = static_cast<HubController *>(params);
    xEventGroupWaitBits(instance->event_group, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY); // wait until zigbee is ready 
    instance->run();
}

void HubController::run(){
    ESP_LOGI(TAG, "Starting hub controller task...");
    controller_data ctrl_data; 
    while (true) {
        if (xQueueReceive(controller_queue, &ctrl_data, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Event received from controller");
            if (ctrl_data.type == DATA_TYPE_DEVICE_JOIN) {
                devices.emplace(ctrl_data.device_id, device_info{});
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
        //plugProtocols.at(ZIGBEE)->open_network();
        ESP_LOGI(TAG, "Device coung in hub map: %d", devices.size());
        for (auto &[key, value] : devices) printf("device id: 0x%016llx\n", key);    
    }
}