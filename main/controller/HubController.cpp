#include "HubController.h"

static const char *TAG = "HUB_CONTROLLER"; 


HubController::HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols) : plugProtocols(protocols){
    xTaskCreate(HubController::runner, "HUB_CONTROLLER", 2048, this, tskIDLE_PRIORITY + 1, &handle);
}

void HubController::runner(void *params){
    auto instance = static_cast<HubController *>(params);
    //wait for eventbits regarding wifi connection or tasknotify 
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