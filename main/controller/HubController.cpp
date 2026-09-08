#include "HubController.h"


static const char *TAG = "HUB_CONTROLLER"; 


HubController::HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events, QueueHandle_t controller_q) : plugProtocols(protocols), event_group(events), controller_queue(controller_q){
    xTaskCreate(HubController::runner, "HUB_CONTROLLER", 2048, this, tskIDLE_PRIORITY + 2, &handle);
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

        if (xQueueReceive(controller_queue, &ctrl_data, pdMS_TO_TICKS(15000)) == pdPASS) {
            switch (ctrl_data.type) 
            {
            case DATA_TYPE_THRESHOLD_LOW:
                ESP_LOGI(TAG, "new low threshold received: %.2f.", ctrl_data.data.threshold);
                threshold_low = ctrl_data.data.threshold;
                check_low_thresholds();
                break;
            case DATA_TYPE_THRESHOLD_MED:
                ESP_LOGI(TAG, "new medium threshold received: %.2f.", ctrl_data.data.threshold); 
                threshold_medium = ctrl_data.data.threshold;
                check_medium_thresholds();
                break; 
            case DATA_TYPE_ELEC_PRICE:
                ESP_LOGI(TAG, "new electricity price received %.2f.", ctrl_data.data.electricity_price);
                current_electricity_price = ctrl_data.data.electricity_price;
                check_low_thresholds();
                check_medium_thresholds();
                request_energy_consumption_values(); // these will be checked every 15mins synced with electrical prices 
                break;
            default:
                handle_zigbee_events(ctrl_data);
                break;
            }
        }
        //somehow check if measurements have not been updated in specific amount of time -> request these
        // 
        // TODO: add timestamps to events from coordinator side -> have a function where we check that we have received responses from plugs in some time period -> online/offline update -> maybe check with software timer IF possible 
        // TODO: what do we do in case of wi-fi connection loss? 
    }
}

void HubController::handle_zigbee_events(controller_data &data){

    auto it = devices.find(data.device_id);
    deviceInfo *dev = (it != devices.end()) ? &it->second : nullptr; 

    switch(data.type)
    {
    case DATA_TYPE_DEVICE_JOIN:
        devices.emplace(data.device_id, device_info{.priority = 1});
        ESP_LOGI(TAG, "New device received by Hub");
        // send to ui 
        break;
    case DATA_TYPE_DEVICE_LEFT:
        devices.erase(data.device_id);
        ESP_LOGI(TAG, "Device erased from Hub map.");
        // send to ui
        break;
    case DATA_TYPE_POWER:
        if (dev != nullptr){
            dev->power = data.data.power; 
            ESP_LOGI(TAG, "Power update %.2f", data.data.power);
            //send to ui
        }  
        break;
    case DATA_TYPE_ENERGY:
        if (dev != nullptr) {
            dev->energy_consumption = data.data.energy_consumption;
            ESP_LOGI(TAG, "Energy update %.2f", data.data.energy_consumption);
            //send to ui 
        } 
        break;
    case DATA_TYPE_CURRENT:
        if (dev != nullptr) {
            dev->current = data.data.current;
            ESP_LOGI(TAG, "Current update %.2f", data.data.current);
            //only cloud 
        } 
        break;
    case DATA_TYPE_VOLTAGE:
        if (dev != nullptr) {
            dev->voltage = data.data.current;
            ESP_LOGI(TAG, "voltage update %.2f", data.data.voltage);
            //only cloud
        }  
        break;
    case DATA_TYPE_SET_ON:
        if (dev != nullptr) {
            dev->on = data.data.set_on;
            ESP_LOGI(TAG, "on/off state update %s", data.data.set_on ? "ON" : "OFF");
            if (dev->on) request_electrical_values(); // request values only from specific device which have been turned on
            // if on -> ask electrical values -> off power 0 
            //send to ui 
        }  
        break;
    case DATA_TYPE_PRIORITY:
        if (dev != nullptr) {
            dev->priority = data.data.priority;
            ESP_LOGI(TAG, "Priority update");
        }
        break;
    case DATA_TYPE_REPORTING:
        // automatic reporting about on/off state 
        break;
    default:
        break;
    }
}

void HubController::check_low_thresholds(){
    ESP_LOGI(TAG, "checking low threshold");
    if (current_electricity_price > threshold_low) {
        for (auto [key, value] : devices) {
            if (value.priority == 1){
                plugProtocols.at(ZIGBEE)->set_plug_off(key);
                ESP_LOGW(TAG, "Turning priority level: 1 device off");
            }
        }
    } else {
        for (auto [key, value] : devices) {
            if (value.priority == 1) {
                plugProtocols.at(ZIGBEE)->set_plug_on(key);
                ESP_LOGW(TAG, "Turning priority level: 1 device on");
            }
        }
    }
}

void HubController::check_medium_thresholds(){
    ESP_LOGI(TAG, "chekcing med threshold");
    if (current_electricity_price > threshold_medium) {
        for (auto [key, value] : devices) {
            if (value.priority == 2) plugProtocols.at(ZIGBEE)->set_plug_off(key);
        }
    } else {
        for (auto [key,value] : devices) {
            if (value.priority == 2) plugProtocols.at(ZIGBEE)->set_plug_on(key); 
        }
    }
}

void HubController::request_energy_consumption_values(){
    ESP_LOGI(TAG, "requesting energy consumption");
    for (auto [key, value] : devices) {
        plugProtocols.at(ZIGBEE)->request_energy_consumption_values(key);
    }
}

void HubController::request_electrical_values(){
    ESP_LOGI(TAG, "reqeusting electrical values"); 
    for (auto [key, value] : devices) {
        plugProtocols.at(ZIGBEE)->request_electrical_values(key); 
    }
}
