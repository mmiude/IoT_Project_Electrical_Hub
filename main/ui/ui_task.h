#ifndef UI_TASK_H
#define UI_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

//NOTE; mirrors the dummy task in main.cpp! 

struct UiTaskParams {

    QueueHandle_t controller_queue;
    QueueHandle_t ui_queue;
    EventGroupHandle_t events;

};

void ui_task(void *params);

#endif // UI_TASK_H
