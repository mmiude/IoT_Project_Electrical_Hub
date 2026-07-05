#include <iostream>

#include "esp_log.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_gateway.h"
#include "ZigbeeCoordinator.h"


extern "C" void app_main(void)
{
    ZigbeeCoordinator coordinator; 
    std::cout << "BOOT" << std::endl; 
    
}