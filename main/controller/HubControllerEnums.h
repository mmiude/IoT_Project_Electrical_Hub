#ifndef HUBCONTROLLERENUMS_H
#define HUBCONTROLLERENUMS_H

#include <string>

#define ZIGBEE_STACK_READY BIT3

enum ProtocolIndex {
    ZIGBEE
};

typedef struct device_info {
    std::string device_name{}; 
    int priority{};
    bool on{};
    bool online{}; 
    float power{};
    float energy_consumption{};
} deviceInfo;

typedef enum {
    DATA_TYPE_DEVICE_JOIN,
    DATA_TYPE_POWER,
    DATA_TYPE_ENERGY,
    DATA_TYPE_SET_ON,
} data_type_t;

typedef struct controller_queue_info {
    uint64_t device_id;
    data_type_t type;

    union data_ {
        float power;
        float energy_consumption;
        bool set_on;
    } data;

} controller_data; 

#endif //HUBCONTROLLERENUMS_H