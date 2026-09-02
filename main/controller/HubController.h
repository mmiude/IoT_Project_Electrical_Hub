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
    static void runner(void *params);
    void run();

    std::vector<std::shared_ptr<IDeviceProtocol>> plugProtocols;
    EventGroupHandle_t event_group;
    QueueHandle_t controller_queue; 
    
    TaskHandle_t handle; 
    //QueueHandle_t controllerQueue;
    //QueueHandle_t displayQueue; 
    std::map<uint64_t, deviceInfo> devices;
    
    float threshold_low;
    float threshold_medium; 
    float current_electricity_price; 
    

};

#endif //HUBCONTROLLER_H