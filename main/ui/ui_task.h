#ifndef UI_TASK_H
#define UI_TASK_H

#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "DeviceInfoStorage.h"
#include "ui_model.h"

// owns the local ui: creates its own task, starts lvgl + screens once

// zigbee is ready and feeds controller messages from ui_queue into the uimodel
class UiTask {
public:
    UiTask(QueueHandle_t controller_queue, QueueHandle_t ui_queue, EventGroupHandle_t events, std::shared_ptr<DeviceInfoStorage<UiDeviceRecord>> storage);

    UiModel &model() { return ui_model; }

private:
    static void runner(void *params);
    void run();

    QueueHandle_t ui_queue;
    EventGroupHandle_t event_group;
    UiModel ui_model;
    TaskHandle_t handle{nullptr};
};

#endif // UI_TASK_H
