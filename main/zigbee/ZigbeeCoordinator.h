#ifndef ZIGBEECONTROLLER_H
#define ZIGBEECONTROLLER_H

#include <map>
#include "zigbee_gateway.h"
#include "smartPlugInfo.h"


//this class inherits controllerInterface class and subject class 
class ZigbeeCoordinator {
public:
    ZigbeeCoordinator();
    //virtual function definitons 
    void get_energy_consumption(uint16_t short_addr);
    void get_electrical_values(uint16_t short_addr);
    int check_device_count();
    void toggle_smart_plug(uint16_t short_addr); 

private: 
    static void runner(void *params);
    void run();

    TaskHandle_t handle;
    QueueHandle_t event_queue_t = NULL;

    //device map/vector/something to keep track of smart plugs
    std::map<uint16_t, smartPlug> devices;
    //uint16_t binding_short_addr; // temporary solutions... ugly... would be great to get rid of some point...

    //commands to smart plugs 
    ezb_err_t read_electrical_measurement_multipliers(uint16_t dst_addr, uint8_t dst_ep);
    ezb_err_t read_electrical_measurement_values(uint16_t dst_addr, uint8_t dst_ep);
    ezb_err_t read_energy_consumption_multipliers(uint16_t dst_addr, uint8_t dst_ep);
    ezb_err_t read_energy_consumption_value(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t read_plug_on_off_state(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t send_toggle_smart_plug(uint16_t dst_addr, uint8_t dst_ep);
    esp_err_t send_configure_reporting(uint16_t dst_addr, uint8_t dst_ep);

    //binding methods 
    ezb_err_t zdo_find_smart_plug_device(uint16_t dst_addr);
    static void zdo_find_smart_plug_device_result(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);
    ezb_err_t zdo_bind_smart_plug_device(uint16_t dst_short_addr, uint8_t dst_ep);
    static void zdo_bind_smart_plug_result(const ezb_zdp_bind_req_result_t *result, void *user_ctx);

};

#endif //ZIGBEECONTROLLER_H