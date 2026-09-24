#ifndef SYSTEMHEALTH_H
#define SYSTEMHEALTH_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

class SystemHealth {
public:
    SystemHealth(EventGroupHandle_t system_events, QueueHandle_t controller_q);
    ~SystemHealth() = default; 

private: 
    static void runner(void *params);
    void run(); 

    EventGroupHandle_t events;
    QueueHandle_t controller_queue; 
    TaskHandle_t task_handle; 

};

#endif //SYSTEMHEALTH_H