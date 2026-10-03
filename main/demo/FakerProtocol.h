#ifndef FAKERPROTOCOL_H
#define FAKERPROTOCOL_H

#include "IDeviceProtocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "HubControllerEnums.h"
#include "DeviceInfoStorage.h"
#include <map>
#include <memory>

typedef struct FakeDevice {
    uint64_t dev_id;
    float active_power;
    float idle_power; 
    bool is_on;
    float total_energy_consumption; 
    TickType_t last_energy_update; 
} f_dev;

typedef enum FakeDataType {
    ENERGY_CONSUMPTION,
    VOLTAGE,
    CURRENT,
    POWER,
    STATE,
} f_data_type;

typedef struct FakerData {
    f_data_type type;
    uint64_t dev_id; 
    union {
        float f_value;
        int i_value;
        bool flag;
    } data; 
} f_data; 

class FakerProtocol : public IDeviceProtocol {

public: 
    FakerProtocol(QueueHandle_t controller_q, EventGroupHandle_t e_bits, std::shared_ptr<DeviceInfoStorage<f_dev>> storage);
        
    void request_energy_consumption_values(uint64_t device_id) override;
    void request_electrical_values(uint64_t device_id) override;
    void request_on_off_state(uint64_t device_id) override;
    void toggle_plug(uint64_t device_id) override;
    void set_plug_on(uint64_t device_id) override;
    void set_plug_off(uint64_t device_id) override;
    void open_network() override;
    void delete_device(uint64_t device_id) override; 

private: 
    static void runner(void *params);
    void run();

    QueueHandle_t controller_queue;
    EventGroupHandle_t event_group; 
    std::shared_ptr<DeviceInfoStorage<f_dev>> dev_storage; 
    QueueHandle_t f_event_queue; 
    TaskHandle_t handle; 

    std::map<uint64_t, f_dev> f_devices; 
    void init_demo_devices();

    float get_voltage(const f_dev &dev);
    float get_power(const f_dev &dev);
    float get_current(const f_dev &dev);
    float get_energy_consumption(f_dev &dev);

    float get_noise(float per);

    f_dev* find_dev(uint64_t id);
 
};

#endif //FAKERPROTOCOL_H

