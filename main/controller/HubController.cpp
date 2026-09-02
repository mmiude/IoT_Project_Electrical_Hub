#include "HubController.h"

//#include "zigbee_gateway.h"

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

    TickType_t current_ticks = xTaskGetTickCount(); 
    TickType_t tick_interval = pdMS_TO_TICKS(30000);

    while (true) {
        if (xQueueReceive(controller_queue, &ctrl_data, pdMS_TO_TICKS(30000)) == pdPASS) {
            handle_zigbee_events(ctrl_data);
            for (auto [key, value] : devices){
                printf("**** DEVICE INFO****\n");
                printf("Device if: 0x%016llx\n", key);
                printf("current: %.4f\n", value.current);
                printf("voltage: %.2f\n", value.voltage);
                printf("power: %.2f\n", value.power);
                printf("energy: %.2f\n", value.energy_consumption);
                printf("state: %d\n", value.on); 
            }
        }
        if (xTaskGetTickCount() - current_ticks >= tick_interval) {
            for (auto [key, value] : devices) {
                plugProtocols.at(ZIGBEE)->request_electrical_values(key);
                plugProtocols.at(ZIGBEE)->request_energy_consumption_values(key);
            }
            current_ticks = xTaskGetTickCount(); 
        }
    }
}

void HubController::handle_zigbee_events(controller_data &data){

    auto it = devices.find(data.device_id);
    deviceInfo *dev = (it != devices.end()) ? &it->second : nullptr; 

    switch(data.type)
    {
    case DATA_TYPE_DEVICE_JOIN:
        devices.emplace(data.device_id, device_info{});
        ESP_LOGI(TAG, "New device received by Hub");
        break;
    case DATA_TYPE_DEVICE_LEFT:
        devices.erase(data.device_id);
        ESP_LOGI(TAG, "Device erased from Hub map.");
        break;
    case DATA_TYPE_POWER:
        if (dev != nullptr){
            dev->power = data.data.power; 
            ESP_LOGI(TAG, "Power update %.2f", data.data.power);
        }  
        break;
    case DATA_TYPE_ENERGY:
        if (dev != nullptr) {
            dev->energy_consumption = data.data.energy_consumption;
            ESP_LOGI(TAG, "Energy update %.2f", data.data.energy_consumption);
        } 
        break;
    case DATA_TYPE_CURRENT:
        if (dev != nullptr) {
            dev->current = data.data.current;
            ESP_LOGI(TAG, "Current update %.2f", data.data.current);
        } 
        break;
    case DATA_TYPE_VOLTAGE:
        if (dev != nullptr) {
            dev->voltage = data.data.current;
            ESP_LOGI(TAG, "voltage update %.2f", data.data.voltage);
        }  
        break;
    case DATA_TYPE_SET_ON:
        if (dev != nullptr) {
            dev->on = data.data.set_on;
            ESP_LOGI(TAG, "on/off state update %s", data.data.set_on ? "ON" : "OFF"); 
        }  
        break;
    case DATA_TYPE_REPORTING:
        break;
    default:
        break;
    }
}