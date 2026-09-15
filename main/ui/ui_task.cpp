#include "ui_task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "screen_manager.h"
#include "HubControllerEnums.h"

//NOTE; mirrors the dummy task in main.cpp!

static const char *TAG = "UI_TASK";

void ui_task(void *params)
{
    auto *p = static_cast<UiTaskParams *>(params);

    xEventGroupWaitBits(p->events, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "UI task started, Zigbee stack ready");

    lvgl_port_init();
    screen_manager_init();

    controller_data msg;
    while (true) {
        if (xQueueReceive(p->ui_queue, &msg, pdMS_TO_TICKS(5)) == pdPASS) {
            switch (msg.type) {
                case DATA_TYPE_DEVICE_JOIN:
                    ESP_LOGI(TAG, "device join: 0x%016llx", msg.device_id);
                    break;
                case DATA_TYPE_DEVICE_LEFT:
                    ESP_LOGI(TAG, "device left: 0x%016llx", msg.device_id);
                    break;
                case DATA_TYPE_POWER:
                    ESP_LOGI(TAG, "dev 0x%016llx power: %.2f", msg.device_id, msg.data.value);
                    break;
                case DATA_TYPE_ONLINE_STATE:
                    ESP_LOGI(TAG, "dev 0x%016llx online: %s", msg.device_id, msg.data.flag ? "yes" : "no");
                    break;
                default:
                    ESP_LOGI(TAG, "dev 0x%016llx: unhandled type %d", msg.device_id, (int)msg.type);
                    break;
            }
        }
        lv_timer_handler();
    }
}