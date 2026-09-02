#ifndef ZIGBEECONTROLLER_H
#define ZIGBEECONTROLLER_H

#include <map>
#include "zigbee_gateway.h"
#include "smartPlugInfo.h"
#include "IDeviceProtocol.h"

#define DEVICE_SIGN_READY   BIT2 


//this class inherits controllerInterface class and subject class 
class ZigbeeCoordinator : public IDeviceProtocol {
public:
    ZigbeeCoordinator(QueueHandle_t controller_queue, EventGroupHandle_t events);

    void request_energy_consumption_values(uint64_t device_id) override;
    void request_electrical_values(uint64_t device_id) override;
    void request_on_off_state(uint64_t device_id) override;
    void toggle_plug(uint64_t device_id) override;
    void set_plug_on(uint64_t device_id) override;
    void set_plug_off(uint64_t device_id) override;
    void open_network() override; 

    int check_device_count();

private: 
    static void runner(void *params);
    void run();

    QueueHandle_t event_queue_t = NULL;
    QueueHandle_t controller_queue;
    EventGroupHandle_t event_group; 
    TaskHandle_t task_handle;
    TaskHandle_t gateway_task_handle;

    std::map<uint64_t, smartPlug> devices;

    smartPlug* find_plug(uint64_t ieee_addr);
    //commands to smart plugs 
    ezb_err_t read_electrical_measurement_multipliers(uint16_t dst_addr, uint8_t dst_ep);
    ezb_err_t read_electrical_measurement_values(uint16_t dst_addr, uint8_t dst_ep);
    ezb_err_t read_energy_consumption_multipliers(uint16_t dst_addr, uint8_t dst_ep);
    ezb_err_t read_energy_consumption_value(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t read_plug_on_off_state(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t send_toggle_smart_plug(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t send_on_smart_plug(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t send_off_smart_plug(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t send_configure_reporting(uint16_t dst_addr, uint8_t dst_ep);

};

#endif //ZIGBEECONTROLLER_H