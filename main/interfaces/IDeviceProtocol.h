#ifndef IDEVICEPROTOCOL_H
#define IDEVICEPROTOCOL_H

#include <cstdint>
//abstract class to create interface between controller and different protocols -> mainly for future development 

class IDeviceProtocol {
public:
    virtual ~IDeviceProtocol() = default; 

    // virtual function here
    virtual void request_energy_consumption_values(uint64_t device_id) = 0;
    virtual void request_electrical_values(uint64_t device_id) = 0;
    virtual void request_on_off_state(uint64_t device_id) = 0;
    virtual void toggle_plug(uint64_t device_id) = 0;
    virtual void set_plug_on(uint64_t device_id) = 0;
    virtual void set_plug_off(uint64_t device_id) = 0;
    virtual void open_network() = 0; 
    
};

#endif //IDEVICEPROTOCOL_H