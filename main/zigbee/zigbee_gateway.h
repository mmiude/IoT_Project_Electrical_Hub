
#pragma once
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "alarm_timer.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"


#define ESP_ZIGBEE_PRIMARY_CHANNEL_MASK   ((1U << 13))
#define ESP_ZIGBEE_SECONDARY_CHANNEL_MASK (0x07FFF800U)

#define ESP_ZIGBEE_CUSTOM_GATEWAY_EP_ID (10)

#define ESP_ZIGBEE_STORAGE_PARTITION_NAME "zb_storage"

#define ESP_MANUFACTURER_NAME "\x09" "ESPRESSIF"
#define ESP_MODEL_IDENTIFIER "\x07" CONFIG_IDF_TARGET

#define ESP_ZIGBEE_ZC_CONFIG()                          \
    {                                                   \
        .device_type = EZB_NWK_DEVICE_TYPE_COORDINATOR, \
        .install_code_policy = false,                   \
        .zczr_config = {                                \
            .max_children = 10,                         \
        },                                              \
    }


#define ESP_ZIGBEE_PLATFORM_CONFIG()                                 \
    {                                                                \
        .storage_partition_name = ESP_ZIGBEE_STORAGE_PARTITION_NAME, \
        .radio_config = {                                            \
            .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE,              \
        },                                                           \
    }

    
#define ESP_ZIGBEE_DEFAULT_CONFIG()                      \
    {                                                    \
        .device_config = ESP_ZIGBEE_ZC_CONFIG(),         \
        .platform_config = ESP_ZIGBEE_PLATFORM_CONFIG(), \
    };


typedef enum {
    ZIGBEE_EVENT_DEVICE_JOINED,
    ZIGBEE_EVENT_DEVICE_LEFT,
    ZIGBEE_EVENT_ONOFF_REPORT,
    ZIGBEE_EVENT_POWER_REPORT,
    ZIGBEE_EVENT_VOLTAGE_REPORT,
    ZIGBEE_EVENT_CURRENT_REPORT,
    ZIGBEE_EVENT_SUMMATION_REPORT,
} zigbee_event_type;

typedef struct {
    zigbee_event_type type;
    uint16_t short_address;

    union zigbee_gateway
    {
        uint8_t end_point;
        bool is_on;
        int16_t raw_power;
        uint16_t raw_voltage;
        uint16_t raw_current;
        uint64_t raw_summation;
    } data;

} zigbee_event;

//gatway creates and owns the queue for signal passing (from gateway to coordinator)

#ifdef __cplusplus
extern "C" {
#endif

QueueHandle_t zigbee_gateway_get_queue(void);
void esp_zigbee_stack_main_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

