#include "HubController.h"


static const char *TAG = "HUB_CONTROLLER"; 


HubController::HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events, QueueHandle_t controller_q) : plugProtocols(protocols), event_group(events), controller_queue(controller_q){
    timer_handle = xTimerCreate("DATA_REQ_TIMER", pdMS_TO_TICKS(15000), pdTRUE, this, dataRequestTimerCallback);
    xTaskCreate(HubController::runner, "HUB_CONTROLLER", 2048, this, tskIDLE_PRIORITY + 2, &handle);
}

void HubController::runner(void *params){
    auto instance = static_cast<HubController *>(params);
    xEventGroupWaitBits(instance->event_group, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY); // wait until zigbee is ready 
    instance->run();
}

void HubController::dataRequestTimerCallback(TimerHandle_t xTimer){
    auto instance = static_cast<HubController *>(pvTimerGetTimerID(xTimer));
    controller_data ctrl_data = {.device_id = 0, .type = DATA_TYPE_REQUEST_ELEC_VALUES}; 
    xQueueSendToBack(instance->controller_queue, &ctrl_data, 0);
}

void HubController::run(){
    ESP_LOGI(TAG, "Starting hub controller task...");
    xTimerStart(timer_handle, 0);
    controller_data ctrl_data;
    
    while (true) {

        if (xQueueReceive(controller_queue, &ctrl_data, portMAX_DELAY) == pdPASS) {
            switch (ctrl_data.type) 
            {
            case DATA_TYPE_THRESHOLD_LOW:
                ESP_LOGI(TAG, "new low threshold received: %.2f.", ctrl_data.data.value);
                threshold_low = ctrl_data.data.value;
                check_low_thresholds();
                break;
            case DATA_TYPE_THRESHOLD_MED:
                ESP_LOGI(TAG, "new medium threshold received: %.2f.", ctrl_data.data.value); 
                threshold_medium = ctrl_data.data.value;
                check_medium_thresholds();
                break; 
            case DATA_TYPE_PRIORITY:
                // should we add the device to controllers map at this point once priority received from ui? -> from ui user confirms that the device should be added
                ESP_LOGI(TAG, "new device priority recieved"); 
                break;
            case DATA_TYPE_ELEC_PRICE:
                ESP_LOGI(TAG, "new electricity price received %.2f.", ctrl_data.data.value);
                current_electricity_price = ctrl_data.data.value;
                check_thresholds();
                break;
            case DATA_TYPE_REQUEST_ELEC_VALUES: // this comes every 15sec 
                ESP_LOGI(TAG, "requesting electrical values.");
                periodic_device_check();
                break;
            case DATA_TYPE_COMMAND:
                command_handler(ctrl_data);
                break;
            default:
                handle_zigbee_events(ctrl_data);
                break;
            }
        } 
    }
}

void HubController::handle_zigbee_events(controller_data &data){

    auto it = devices.find(data.device_id);
    deviceInfo *dev = (it != devices.end()) ? &it->second : nullptr; 

    switch(data.type)
    {
    case DATA_TYPE_DEVICE_JOIN:
        devices.emplace(data.device_id, device_info{
            .priority = 1, // this will be taken off
            .online = true,
            .periodic_check_count = 0,
            .last_seen = xTaskGetTickCount(),
        });
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
            dev->power = data.data.value;
            dev->last_seen = xTaskGetTickCount(); 
            ESP_LOGI(TAG, "Power update %.2f", data.data.value);
            //send to ui
        }  
        break;
    case DATA_TYPE_ENERGY:
        if (dev != nullptr) {
            dev->energy_consumption = data.data.value;
            dev->last_seen = xTaskGetTickCount();
            ESP_LOGI(TAG, "Energy update %.2f", data.data.value);
            //send to ui 
        } 
        break;
    case DATA_TYPE_CURRENT:
        if (dev != nullptr) {
            dev->current = data.data.value;
            dev->last_seen = xTaskGetTickCount();
            ESP_LOGI(TAG, "Current update %.2f", data.data.value);
            //only cloud 
        } 
        break;
    case DATA_TYPE_VOLTAGE:
        if (dev != nullptr) {
            dev->voltage = data.data.value;
            dev->last_seen = xTaskGetTickCount();
            ESP_LOGI(TAG, "voltage update %.2f", data.data.value);
            //only cloud
        }  
        break;
    case DATA_TYPE_SET_ON:
        if (dev != nullptr) {
            dev->on = data.data.flag;
            dev->last_seen = xTaskGetTickCount();
            ESP_LOGI(TAG, "on/off state update %s", data.data.flag ? "ON" : "OFF");
            if (dev->on) plugProtocols.at(ZIGBEE)->request_electrical_values(data.device_id);
            //send to ui 
        }  
        break;
    case DATA_TYPE_REPORTING:
        if (dev != nullptr) {
            dev->reporting_on = data.data.flag;
            ESP_LOGI(TAG, "supports reporting %s", data.data.flag ? "YES" : "NO");
        }
        break;
    case DATA_TYPE_SUPPORTS_METERING:
        if (dev != nullptr){
            dev->support_energy_consumption = data.data.flag;
            ESP_LOGI(TAG, "supports energy consumption %s", data.data.flag ? "YES" : "NO");
            // send to ui 
        }
        break;
    default:
        break;
    }
}

void HubController::check_low_thresholds(){
    ESP_LOGI(TAG, "checking low threshold");
    for (auto &[key, dev] : devices) {
        if (dev.priority == 1) {
            if (current_electricity_price > threshold_low) {
                if (dev.on) plugProtocols.at(ZIGBEE)->set_plug_off(key);
            } else {
                if (!dev.on) plugProtocols.at(ZIGBEE)->set_plug_on(key);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void HubController::check_medium_thresholds(){
    ESP_LOGI(TAG, "checking med threshold");
    for (auto &[key, dev] : devices) {
        if (dev.priority == 2) {
            if (current_electricity_price > threshold_medium) {
                if (dev.on) plugProtocols.at(ZIGBEE)->set_plug_off(key);
            } else {
                if (!dev.on) plugProtocols.at(ZIGBEE)->set_plug_on(key);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void HubController::check_thresholds(){
    ESP_LOGI(TAG, "checking both thresholds");

    for (auto &[key, dev] : devices) {
        if (dev.priority == 2) {
            if (current_electricity_price > threshold_medium) {
                if (dev.on) plugProtocols.at(ZIGBEE)->set_plug_off(key);
            } else {
                if (!dev.on) plugProtocols.at(ZIGBEE)->set_plug_on(key);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        else if (dev.priority == 1) {
            if (current_electricity_price > threshold_low) {
                if (dev.on) plugProtocols.at(ZIGBEE)->set_plug_off(key);
            } else {
                if (!dev.on) plugProtocols.at(ZIGBEE)->set_plug_on(key);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        else ESP_LOGI(TAG, "higher priority level device then 2. Not effected by thresholds.");
    }
}

void HubController::command_handler(controller_data &data){
    /*auto it = devices.find(data.device_id);
    if (it == devices.end()) {
        ESP_LOGE(TAG, "DEVICE NOT ON CONTROLLER MAP"); 
        // device requested is not on controllers list -> must be deleted from ui as well... should never happen but should we have this check anyways?
    }*/
    switch(data.data.command) {
        case TOGGLE_PLUG:
            plugProtocols.at(ZIGBEE)->toggle_plug(data.device_id);
            break;
        case PLUG_ON: 
            plugProtocols.at(ZIGBEE)->set_plug_on(data.device_id);
            break;
        case PLUG_OFF:
            plugProtocols.at(ZIGBEE)->set_plug_off(data.device_id);
            break; 
        case OPEN_NETWORK:
            plugProtocols.at(ZIGBEE)->open_network(); 
            // notify leds 
            break;
        default:
            ESP_LOGE(TAG, "Unknown command request");
            break;
    }
}

void HubController::periodic_device_check(){
    
    ESP_LOGI(TAG, "periodic device check");
    
    for (auto& [key, dev] : devices) {
        ++dev.periodic_check_count;
        // request electrical values.
        plugProtocols.at(ZIGBEE)->request_electrical_values(key);

        vTaskDelay(pdMS_TO_TICKS(5)); // small delay between requests so Zigbee network won't get angry. 

        // request plug state if reporting is not on for some reason.
        if (!dev.reporting_on) plugProtocols.at(ZIGBEE)->request_on_off_state(key);

        // request energy consumption valuse every 5 mins
        if (dev.periodic_check_count > 20 && dev.support_energy_consumption){ 
            ESP_LOGI(TAG, "requesting energy consumption values");
            plugProtocols.at(ZIGBEE)->request_energy_consumption_values(key);
            dev.periodic_check_count = 0; 
        } 
        
        // aliveness check
        if (uint32_t elapsed_time = ((xTaskGetTickCount() - dev.last_seen) * portTICK_PERIOD_MS) ; elapsed_time > 30000) {
            ESP_LOGE(TAG, "Device: 0x%016llx is dead! Last seen %d ms ago", key, elapsed_time);
            dev.online = false;
        } else dev.online = true; 

        vTaskDelay(pdMS_TO_TICKS(10)); // small delay so Zigbee network won't get angry. 
    }
}