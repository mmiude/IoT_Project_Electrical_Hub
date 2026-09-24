#ifndef HUBCONTROLLERENUMS_H
#define HUBCONTROLLERENUMS_H

#include <string>

#define ZIGBEE_STACK_READY BIT3

enum ProtocolIndex {
    ZIGBEE
};

typedef enum {
    Z_NETWORK_OPEN,
    Z_NETWORK_CLOSE,
    Z_NETWORK_UP,
    Z_NETWORK_DOWN
} z_network_status;

typedef enum {
    TOGGLE_PLUG,
    PLUG_ON,
    PLUG_OFF,
    OPEN_NETWORK,
    // hub-side forget only (map + nvs), echoes DATA_TYPE_DEVICE_LEFT to the ui. does NOT ask the
    // zigbee radio to unpair the device - see HubController::remove_device.
    REMOVE_DEVICE
} commands;

typedef struct device_info {
    int priority{};
    bool on{};
    bool online{};
    bool automation_on{};
    bool support_energy_consumption{};
    bool reporting_on{};
    int periodic_check_count{};
    TickType_t last_seen{};
} deviceInfo;

typedef enum {
    // device lifecycle - coming from coordinator 
    DATA_TYPE_DEVICE_JOIN,
    DATA_TYPE_DEVICE_LEFT,
    // measurements and metering/reporting support from coordinator
    DATA_TYPE_POWER,
    DATA_TYPE_ENERGY,
    DATA_TYPE_VOLTAGE, 
    DATA_TYPE_CURRENT,
    DATA_TYPE_SET_ON,
    DATA_TYPE_REPORTING,
    DATA_TYPE_SUPPORTS_METERING,
    DATA_TYPE_NETWORK_OPEN,
    // threshold, priority and electricity price info coming from ui 
    DATA_TYPE_THRESHOLD_LOW,
    DATA_TYPE_THRESHOLD_MED,
    DATA_TYPE_PRIORITY,
    DATA_TYPE_ELEC_PRICE,
    DATA_TYPE_AUTOMATION,
    // commands coming from ui side
    DATA_TYPE_COMMAND,
    // internal for controller - periodic info request from plugs
    DATA_TYPE_REQUEST_ELEC_VALUES,
    // for ui to recieve online info
    DATA_TYPE_ONLINE_STATE,
    // system health information
    DATA_TYPE_NETOWRK_ALIVE,
    DATA_TYPE_WIFI_ONLINE
,
    // ui <-> controller state sync, ui asks once after it has started, controller replays its state to the ui
    // (thresholds, price if known and every device: join, priority, on, online, metering) + ends with SYNC_DONE
    DATA_TYPE_UI_SYNC_REQUEST,
    DATA_TYPE_UI_SYNC_DONE
} data_type_t;

typedef struct controller_queue_info {
    uint64_t device_id;
    data_type_t type;

    union data_ {
        float value;
        int value_int; 
        bool flag;
        commands command; 
    } data;

} controller_data; 

#endif //HUBCONTROLLERENUMS_H