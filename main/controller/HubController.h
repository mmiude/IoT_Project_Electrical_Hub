#ifndef HUBCONTROLLER_H
#define HUBCONTROLLER_H

#include <vector>
#include <memory>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "IDeviceProtocol.h"
#include "HubControllerEnums.h"
#include "DeviceInfoStorage.h"
#include "SystemConfigStorage.h"

class HubController {
public:
    HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events, QueueHandle_t controller_q, QueueHandle_t cloud_q, QueueHandle_t ui_q, std::shared_ptr<DeviceInfoStorage<deviceInfo>> dev_stroage, std::shared_ptr<SystemConfigStorage> config_storage); 

private:
    static void dataRequestTimerCallback(TimerHandle_t xTimer); 
    static void runner(void *params);
    void run();

    std::vector<std::shared_ptr<IDeviceProtocol>> plugProtocols;
    EventGroupHandle_t event_group;
    QueueHandle_t controller_queue;
    QueueHandle_t cloud_queue;
    QueueHandle_t ui_queue; 
    
    TaskHandle_t handle; 
    TimerHandle_t timer_handle;

    std::shared_ptr<DeviceInfoStorage<deviceInfo>> device_info_storage;
    std::shared_ptr<SystemConfigStorage> system_config_storage;
    std::map<uint64_t, deviceInfo> devices;
    
    float threshold_low;
    float threshold_medium; 
    float current_electricity_price; 
    
    void handle_zigbee_events(controller_data &data); 
    void check_low_thresholds();
    void check_medium_thresholds();
    void check_thresholds();
    bool threshold_allows_opening(int priority);
    void command_handler(controller_data &data);
    void periodic_device_check();

    void check_device_map();

};

#endif //HUBCONTROLLER_H