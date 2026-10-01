#ifndef FAKERPROTOCOL_H
#define FAKERPROTOCOL_H

#include "IDeviceProtocol.h"
#include <map>

typedef struct FakeDevice {
    uint64_t dev_id;
    float base_power_w;
    float idle_power_w; 
    bool is_on;
    float energy_consumption; 
} f_dev;

class FakerProtocol : public IDeviceProtocol {

public: 
    FakerProtocol() {  // let's see if this rembers the devices -> would work then 100% same as zigbee coordinator... -> then we would pass it's own nvs namespace 
        init_demo_devices();
    }
    
    void request_energy_consumption_values(uint64_t device_id) override;
    void request_electrical_values(uint64_t device_id) override;
    void request_on_off_state(uint64_t device_id) override;
    void toggle_plug(uint64_t device_id) override;
    void set_plug_on(uint64_t device_id) override;
    void set_plug_off(uint64_t device_id) override;

private: 
    std::map<uint64_t, f_dev> f_devices; 
    void init_demo_devices();
};

#endif //FAKERPROTOCOL_H

