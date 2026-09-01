#ifndef HUBCONTROLLER_H
#define HUBCONTROLLER_H

#include <vector>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "IDeviceProtocol.h"

class HubController {
public:
    HubController(const std::vector<std::shared_ptr<IDeviceProtocol>> &protocols); 

private:
    static void runner(void *params);
    void run();

    std::vector<std::shared_ptr<IDeviceProtocol>> plugProtocols;
    
    TaskHandle_t handle; 
    //QueueHandle_t controllerQueue;
    //QueueHandle_t displayQueue; 

    //probs needs own device map -> need to think about what controller needs to know! 

};

#endif //HUBCONTROLLER_H