#include "HubController.h"


static const char *TAG = "HUB_CONTROLLER"; 


HubController::HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events, QueueHandle_t controller_q, QueueHandle_t cloud_q, QueueHandle_t ui_q, std::shared_ptr<DeviceInfoStorage<deviceInfo>> dev_stroage, std::shared_ptr<SystemConfigStorage> config_storage) : 
plugProtocols(protocols), event_group(events), controller_queue(controller_q), cloud_queue(cloud_q), ui_queue(ui_q), device_info_storage(dev_stroage), system_config_storage(config_storage) {
    timer_handle = xTimerCreate("DATA_REQ_TIMER", pdMS_TO_TICKS(15000), pdTRUE, this, dataRequestTimerCallback);
    xTaskCreate(HubController::runner, "HUB_CONTROLLER", 2048, this, tskIDLE_PRIORITY + 2, &handle);
}

void HubController::attach(std::shared_ptr<Observer> obs) {
    observers.push_back(obs);
}

void HubController::notify(int state) {
    for (const auto &o : observers) o->update(state); 
}

void HubController::runner(void *params){
    auto instance = static_cast<HubController *>(params);
    xEventGroupWaitBits(instance->event_group, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY); // wait until zigbee is ready 
    instance->run();
}

void HubController::dataRequestTimerCallback(TimerHandle_t xTimer){
    auto instance = static_cast<HubController *>(pvTimerGetTimerID(xTimer));
    controller_data ctrl_data = {.device_id = 0, .type = DATA_TYPE_REQUEST_ELEC_VALUES, .data{}}; 
    xQueueSendToBack(instance->controller_queue, &ctrl_data, 0);
}

void HubController::run(){
    ESP_LOGI(TAG, "Starting hub controller task...");
    xTimerStart(timer_handle, 0);
    controller_data ctrl_data;


    device_info_storage->get_all_devices(devices); 
    if (devices.empty()) ESP_LOGI(TAG, "no device info saved on NVS.");
    else check_device_map();
    system_config_storage->get_threshold_levels(threshold_low, threshold_medium); // if there is no values saved these returns zeros 
    ESP_LOGW(TAG, "read following values low: %f, med: %f", threshold_low, threshold_medium);
    
    while (true) {

        if (xQueueReceive(controller_queue, &ctrl_data, portMAX_DELAY) == pdPASS) {
            switch (ctrl_data.type) 
            {
            case DATA_TYPE_THRESHOLD_LOW:
                ESP_LOGI(TAG, "new low threshold received: %.2f.", ctrl_data.data.value);
                threshold_low = ctrl_data.data.value;
                check_low_thresholds();
                system_config_storage->save_low_threshold(ctrl_data.data.value);
                break;
            case DATA_TYPE_THRESHOLD_MED:
                ESP_LOGI(TAG, "new medium threshold received: %.2f.", ctrl_data.data.value); 
                threshold_medium = ctrl_data.data.value;
                check_medium_thresholds();
                system_config_storage->save_med_threshold(ctrl_data.data.value);
                break; 
            case DATA_TYPE_PRIORITY: 
                modify_dev_priority(ctrl_data.device_id, ctrl_data.data.value_int);
                ESP_LOGI(TAG, "new device priority recieved"); 
                break;
            case DATA_TYPE_AUTOMATION:
                modify_dev_automation(ctrl_data.device_id, ctrl_data.data.flag);
                break;
            case DATA_TYPE_ELEC_PRICE:
                ESP_LOGI(TAG, "new electricity price received %.2f.", ctrl_data.data.value);
                current_electricity_price = ctrl_data.data.value;
                price_received = true;
                check_low_thresholds();
                check_medium_thresholds();
                break;
            case DATA_TYPE_REQUEST_ELEC_VALUES: // this comes every 15sec 
                ESP_LOGI(TAG, "requesting electrical values.");
                periodic_device_check();
                printf("map sizes: controller: %d\n", devices.size());
                break;
            case DATA_TYPE_COMMAND:
                command_handler(ctrl_data);
                break;
            case DATA_TYPE_UI_SYNC_REQUEST:
                send_ui_sync();
                break;
            case DATA_TYPE_NETWORK_OPEN:
                if (ctrl_data.data.flag) notify(Z_NETWORK_OPEN);
                else notify(Z_NETWORK_CLOSE); 
                break;
            case DATA_TYPE_NETOWRK_ALIVE:
                if (ctrl_data.data.flag) notify(Z_NETWORK_UP);
                else notify(Z_NETWORK_DOWN);
                break;
            case DATA_TYPE_WIFI_ONLINE:
                if (!ctrl_data.data.flag) // notify ui -> wi-fi connection lost
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
        devices.emplace(data.device_id, deviceInfo{
            .priority = 0, // this will be taken off
            .online = true,
            .automation_on = true,
            .periodic_check_count = 0,
            .last_seen = xTaskGetTickCount(),
        });
        //ESP_LOGI(TAG, "New device received by Hub");
        xQueueSendToBack(ui_queue, &data, 0);
        break; 
    case DATA_TYPE_DEVICE_LEFT:
        devices.erase(data.device_id);
        //ESP_LOGI(TAG, "Device erased from Hub map.");
        xQueueSendToBack(ui_queue, &data, 0);
        device_info_storage->delete_device_from_memory(data.device_id); 
        break;
    case DATA_TYPE_POWER:
        if (dev != nullptr){
            dev->last_seen = xTaskGetTickCount(); 
            //ESP_LOGI(TAG, "Power update %.2f", data.data.value);
            xQueueSendToBack(ui_queue, &data, 0);
        }  
        break;
    case DATA_TYPE_ENERGY:
        if (dev != nullptr) {
            dev->last_seen = xTaskGetTickCount();
            //ESP_LOGI(TAG, "Energy update %.2f", data.data.value);
            xQueueSendToBack(ui_queue, &data, 0);
        } 
        break;
    case DATA_TYPE_CURRENT:
        if (dev != nullptr) {
            dev->last_seen = xTaskGetTickCount();
            ESP_LOGI(TAG, "Current update %.2f", data.data.value);
            //only cloud
        } 
        break;
    case DATA_TYPE_VOLTAGE:
        if (dev != nullptr) {
            dev->last_seen = xTaskGetTickCount();
            ESP_LOGI(TAG, "voltage update %.2f", data.data.value);
            //only cloud
        }  
        break;
    case DATA_TYPE_SET_ON:
        if (dev != nullptr) {
            dev->on = data.data.flag;
            dev->last_seen = xTaskGetTickCount();
            //ESP_LOGI(TAG, "on/off state update %s", data.data.flag ? "ON" : "OFF");
            if (dev->automation_on && !threshold_allows_opening(dev->priority)) plugProtocols.at(ZIGBEE)->set_plug_off(data.device_id); 
            //if (dev->on) plugProtocols.at(ZIGBEE)->request_electrical_values(data.device_id);
            xQueueSendToBack(ui_queue, &data, 0);
        }  
        break;
    case DATA_TYPE_REPORTING:
        if (dev != nullptr) {
            dev->reporting_on = data.data.flag;
            ESP_LOGI(TAG, "supports reporting %s", data.data.flag ? "YES" : "NO");
            device_info_storage->save_device(data.device_id, *dev); 
        }
        break;
    case DATA_TYPE_SUPPORTS_METERING:
        if (dev != nullptr){
            dev->support_energy_consumption = data.data.flag;
            ESP_LOGI(TAG, "supports energy consumption %s", data.data.flag ? "YES" : "NO");
            xQueueSendToBack(ui_queue, &data, 0); 
        }
        break;
    default:
        ESP_LOGE(TAG, "Controller received unknown zigbee data type."); 
        break;
    }
}

void HubController::check_device_map(){
    ESP_LOGI(TAG, "*****INFO READ FROM MEMROY*****");
    for (auto &[key, dev] : devices) {
    printf("Dev id: 0x%016llx, prio: %d, reporting on: %s \n", key, dev.priority, dev.reporting_on ? "YES" : "NO");
    dev.last_seen = 0; 
    }
}

bool HubController::threshold_allows_opening(int priority) {
    if (priority == 1) {
        if (current_electricity_price > threshold_low) return false;
        else return true;
    } 
    else if (priority == 2) {
        if (current_electricity_price > threshold_medium) return false;
        else return true;
    }
    return true;   
}

void HubController::check_low_thresholds(){
    ESP_LOGI(TAG, "checking low threshold");
    for (auto &dev : devices | std::views::filter([] (const auto &dev) {return dev.second.priority == 1 && dev.second.automation_on;})) {
        if (current_electricity_price > threshold_low) {
            plugProtocols.at(ZIGBEE)->set_plug_off(dev.first);
        } else {
            plugProtocols.at(ZIGBEE)->set_plug_on(dev.first);
        }
        vTaskDelay(pdMS_TO_TICKS(10));  
    }
}

void HubController::check_medium_thresholds(){
    ESP_LOGI(TAG, "checking med threshold");
    for (auto &dev : devices | std::views::filter([] (const auto &dev) {return dev.second.priority == 2 && dev.second.automation_on;})) {
        if (current_electricity_price > threshold_medium) {
            plugProtocols.at(ZIGBEE)->set_plug_off(dev.first);
        } else {
            plugProtocols.at(ZIGBEE)->set_plug_on(dev.first);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*void HubController::check_thresholds(){
    ESP_LOGI(TAG, "checking both thresholds. elec price: %.2f, low: %.2f, med: %.2f", current_electricity_price, threshold_low, threshold_medium);

    for (auto &[key, dev] : devices) {
        if (dev.priority == 2 && dev.automation_on) {
            if (current_electricity_price > threshold_medium) {
                if (dev.on){
                    plugProtocols.at(ZIGBEE)->set_plug_off(key);
                    ESP_LOGI(TAG, "setting plug off on threshold check.");
                } 
            } else {
                if (!dev.on){
                    plugProtocols.at(ZIGBEE)->set_plug_on(key);
                    ESP_LOGI(TAG, "setting plug on on threshold check");
                } 
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        else if (dev.priority == 1 && dev.automation_on) {
            if (current_electricity_price > threshold_low) {
                plugProtocols.at(ZIGBEE)->set_plug_off(key);
                ESP_LOGI(TAG, "setting plug off on threshold check");
                
            } else {
                plugProtocols.at(ZIGBEE)->set_plug_on(key);
                ESP_LOGI(TAG, "setting plug on on threshold check");
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        else ESP_LOGI(TAG, "higher priority level device then 2. Not effected by thresholds.");
    }
}*/

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
            break;
        default:
            ESP_LOGE(TAG, "Unknown command request");
            break;
    }
}

void HubController::periodic_device_check(){
    
    ESP_LOGI(TAG, "periodic device check");
    controller_data ctrl_data; 
    
    for (auto &[key, dev] : devices) {
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
            if (dev.online) {
                ctrl_data = {.device_id = key, .type = DATA_TYPE_ONLINE_STATE, .data = {.flag = false}}; // we send to ui only if state has changed
                xQueueSendToBack(ui_queue, &ctrl_data, 0); 
            }
            dev.online = false;
        } else {
            if (!dev.online) {
                ctrl_data = {.device_id = key, .type = DATA_TYPE_ONLINE_STATE, .data = {.flag = true}}; // we send to ui only if state has changed
                xQueueSendToBack(ui_queue, &ctrl_data, 0); 
            }
            dev.online = true; 
        } 
        vTaskDelay(pdMS_TO_TICKS(10)); // small delay so Zigbee network won't get angry. 
    }
}

void HubController::modify_dev_priority(uint64_t dev_id, int priority) {
    auto it = devices.find(dev_id);
    if (it != devices.end()) {
        it->second.priority = priority; 
        device_info_storage->save_device(it->first, it->second);
    } else ESP_LOGE(TAG, "dev not found! no priority modified.");
}

void HubController::modify_dev_automation(uint64_t dev_id, bool state) {
    auto it = devices.find(dev_id);
    if (it != devices.end()) {
        it->second.automation_on = state; 
        device_info_storage->save_device(it->first, it->second);
    } else ESP_LOGE(TAG, "dev not found! no automation flag modified.");
}


// ------ ui 

// short timeout (unlike the normal 0) because the sync burst can be bigger than what the ui queue holds and the ui is draining it 
bool HubController::push_to_ui(controller_data &data){
    return xQueueSendToBack(ui_queue, &data, pdMS_TO_TICKS(50)) == pdPASS;
}

void HubController::send_ui_sync(){
    ESP_LOGI(TAG, "ui requested sync, replaying state.");
    bool ok = true;
    controller_data msg{};

    msg = {.type = DATA_TYPE_THRESHOLD_LOW, .data = {.value = threshold_low}};
    ok &= push_to_ui(msg);
    msg = {.type = DATA_TYPE_THRESHOLD_MED, .data = {.value = threshold_medium}};
    ok &= push_to_ui(msg);
    if (price_received) {
        msg = {.type = DATA_TYPE_ELEC_PRICE, .data = {.value = current_electricity_price}};
        ok &= push_to_ui(msg);
    }

    for (auto &[key, dev] : devices) {
        msg = {.device_id = key, .type = DATA_TYPE_DEVICE_JOIN, .data = {}};
        ok &= push_to_ui(msg);
        msg = {.device_id = key, .type = DATA_TYPE_PRIORITY, .data = {.value_int = dev.priority}};
        ok &= push_to_ui(msg);
        msg = {.device_id = key, .type = DATA_TYPE_SET_ON, .data = {.flag = dev.on}};
        ok &= push_to_ui(msg);
        msg = {.device_id = key, .type = DATA_TYPE_ONLINE_STATE, .data = {.flag = dev.online}};
        ok &= push_to_ui(msg);
        msg = {.device_id = key, .type = DATA_TYPE_SUPPORTS_METERING, .data = {.flag = dev.support_energy_consumption}};
        ok &= push_to_ui(msg);
    }

    // ui removes devices it knows about but are missing from the replay, so only say done if nothing got dropped
    if (ok) {
        msg = {.type = DATA_TYPE_UI_SYNC_DONE, .data = {}};
        push_to_ui(msg);
    } else {
        ESP_LOGE(TAG, "ui queue full during sync, some state was dropped.");
    }
}
