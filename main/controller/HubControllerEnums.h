#ifndef HUBCONTROLLERENUMS_H
#define HUBCONTROLLERENUMS_H

#include <string>

#define ZIGBEE_STACK_READY BIT3

enum ProtocolIndex {
    ZIGBEE
};

typedef enum {
    TOGGLE_PLUG,
    PLUG_ON,
    PLUG_OFF,
    OPEN_NETWORK
} commands;

typedef struct device_info {
    int priority{};
    bool on{};
    bool online{};
    bool automation_on{};
    bool support_energy_consumption{};
    bool reporting_on{};
    float power{};
    float current{};
    float voltage{};
    float energy_consumption{};
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
    // threshold, priority and electricity price info coming from ui 
    DATA_TYPE_THRESHOLD_LOW,
    DATA_TYPE_THRESHOLD_MED,
    DATA_TYPE_PRIORITY,
    DATA_TYPE_ELEC_PRICE,
    // commands coming from ui side
    DATA_TYPE_COMMAND,
    // internal for controller - periodic info request from plugs
    DATA_TYPE_REQUEST_ELEC_VALUES,
    // for ui to recieve online info
    DATA_TYPE_ONLINE_STATE
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