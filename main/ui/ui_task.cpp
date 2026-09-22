#include "ui_task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "screen_manager.h"
#include "HubControllerEnums.h"

static const char *TAG = "UI_TASK";

UiTask::UiTask(QueueHandle_t controller_queue, QueueHandle_t ui_queue, EventGroupHandle_t events, std::shared_ptr<DeviceInfoStorage<UiDeviceRecord>> storage)
    : ui_queue(ui_queue), event_group(events), ui_model(controller_queue, storage) {
    // stack size needs to be big for lvgl (16384 worked in my tests) and priority is idle + 1 since touch
    xTaskCreate(UiTask::runner, "UI_TASK", 16384, this, tskIDLE_PRIORITY + 1, &handle);
}

void UiTask::runner(void *params) {
    static_cast<UiTask *>(params)->run();
}

void UiTask::run() {
    xEventGroupWaitBits(event_group, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "UI task started, Zigbee stack ready");

    ui_model.load(); // named devices from nvs, before the screens are built so they can show them
    lvgl_port_init();
    screen_manager_init(ui_model);

    ui_model.request_sync(); // controller replays thresholds, price and device states

    controller_data msg;
    while (true) {
        if (xQueueReceive(ui_queue, &msg, pdMS_TO_TICKS(5)) == pdPASS) {
            ui_model.handle_message(msg);
        }
        lv_timer_handler();
    }
}
