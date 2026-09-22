#include "zigbee_gateway.h"  // we really need to put them eventbits somewhere smart!! 
#include "HubControllerEnums.h"
#include "SystemHealth.h"

SystemHealth::SystemHealth(EventGroupHandle_t system_events, QueueHandle_t controller_q) : events(system_events), controller_queue(controller_q) {

    xTaskCreate(SystemHealth::runner, "WATCHDOG_TASK", 2048, this, tskIDLE_PRIORITY + 1, &task_handle);
}

void SystemHealth::runner(void *params) {
    auto instance = static_cast<SystemHealth *>(params);
    xEventGroupWaitBits(instance->events, ZIGBEE_STACK_READY, pdFALSE, pdFALSE, portMAX_DELAY);
    instance->run();
}

void SystemHealth::run() {
    controller_data ctrl_data;
    TickType_t last_check_time = xTaskGetTickCount();
    
    while (true) {

        EventBits_t bits = xEventGroupClearBits(events, ZIGBEE_ALIVE_BIT);

        if (!(bits & ZIGBEE_ALIVE_BIT)) {
            ESP_LOGE("SYS_HEALTH", "zigbee dead!");
            ctrl_data = {.device_id = 0, .type = DATA_TYPE_NETOWRK_ALIVE, .data = {.flag = false}};
            xQueueSendToBack(controller_queue, &ctrl_data, 0);
        } else ESP_LOGI("SYS_HEALTH", "zigbee alive!");

        vTaskDelayUntil(&last_check_time, pdMS_TO_TICKS(30000));
    }
}