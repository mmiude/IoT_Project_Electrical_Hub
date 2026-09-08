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

class HubController {
public:
    HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols, EventGroupHandle_t events, QueueHandle_t controller_q); 

private:
    static void dataRequestTimerCallback(TimerHandle_t xTimer); 
    static void runner(void *params);
    void run();

    std::vector<std::shared_ptr<IDeviceProtocol>> plugProtocols;
    EventGroupHandle_t event_group;
    QueueHandle_t controller_queue; 
    
    TaskHandle_t handle; 
    TimerHandle_t timer_handle;
    std::map<uint64_t, deviceInfo> devices;
    
    float threshold_low;
    float threshold_medium; 
    float current_electricity_price; 
    
    void handle_zigbee_events(controller_data &data); 
    void check_low_thresholds();
    void check_medium_thresholds();
    void request_energy_consumption_values();
    void request_electrical_values(); 
    void check_device_aliveness(); 
    void check_on_off_state(); 

    // calculate energy_consumption
    // calculate price (or if these to can be together)
};

#endif //HUBCONTROLLER_H