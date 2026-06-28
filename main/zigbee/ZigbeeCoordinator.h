#ifndef ZIGBEECONTROLLER_H
#define ZIGBEECONTROLLER_H

#include "zigbee_gateway.h"

class ZigbeeCoordinator {
public:
    ZigbeeCoordinator();

private: 
    static void runner(void *params);
    void run();

    TaskHandle_t handle;
    QueueHandle_t event_queue;

    //private members 

    //methods
};

#endif //ZIGBEECONTROLLER_H