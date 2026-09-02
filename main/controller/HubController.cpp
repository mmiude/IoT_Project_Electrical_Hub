#include "HubController.h"
#include "zigbee_gateway.h"

static const char *TAG = "HUB_CONTROLLER"; 


HubController::HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events) : plugProtocols(protocols), event_group(events){
    xTaskCreate(HubController::runner, "HUB_CONTROLLER", 2048, this, tskIDLE_PRIORITY + 1, &handle);
}

void HubController::runner(void *params){
    auto instance = static_cast<HubController *>(params);
    xEventGroupWaitBits(instance->event_group, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY); // wait until zigbee is ready 
    instance->run();
}

void HubController::run(){
    ESP_LOGI(TAG, "Starting hub controller task...");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        plugProtocols.at(0)->open_network();
        vTaskDelay(pdMS_TO_TICKS(10000));     
    }
}