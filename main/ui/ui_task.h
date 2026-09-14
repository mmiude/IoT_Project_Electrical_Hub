#ifndef UI_TASK_H
#define UI_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

struct UiTaskParams {

    //ideas:
    //ui -> controller: threshholds: priority, elect price, commands?
    //controller -> ui: device lifecycle, measurements, online state

};

void ui_task(void *params);

#endif // UI_TASK_H