#include "zigbee_gateway.h"  // we really need to put them eventbits somewhere smart!! 
#include "HubControllerEnums.h"
#include "SystemHealth.h"

SystemHealth::SystemHealth(EventGroupHandle_t system_events, QueueHandle_t controller_q) : events(system_events), controller_queue(controller_q) {

    xTaskCreate(SystemHealth::runner, "WATCHDOG_TASK", 2048, this, tskIDLE_PRIORITY + 1, &task_handle);

    z_previous_state = false; 
}

void SystemHealth::runner(void *params) {
    auto instance = static_cast<SystemHealth *>(params);
    instance->run();
}

void SystemHealth::run() {
    controller_data ctrl_data;

    const EventBits_t ALL_GOOD_BITS = ZIGBEE_ALIVE_BIT; // then we would add here | WI_FI_ALIVE_BIT and so on...
    while (true) {
        if (EventBits_t bits = xEventGroupWaitBits(events, ZIGBEE_ALIVE_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(30000)); (bits & ALL_GOOD_BITS) == ALL_GOOD_BITS){
            ESP_LOGI("WATCHDOG", "all good in the hood!");
            if (!z_previous_state) {
                ctrl_data = {.device_id = 0, .type = DATA_TYPE_ONLINE_STATE, .data = {.flag = true}};
                xQueueSendToBack(controller_queue, &ctrl_data, 0);
                z_previous_state = true; 
            }
        } else {
            if (!(bits & ZIGBEE_ALIVE_BIT)) {
                ESP_LOGW("WATCHDOG", "Zigbee not alive!"); 
                if (z_previous_state) {
                    ctrl_data = {.device_id = 0, .type = DATA_TYPE_ONLINE_STATE, .data = {.flag = false}};
                    xQueueSendToBack(controller_queue, &ctrl_data, 0); 
                    z_previous_state = false; 
                }
            }
            xEventGroupClearBits(events, ALL_GOOD_BITS); 
        }
    }
}