#ifndef IP_STACK_H
#define IP_STACK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1


bool get_efuse_mac(uint8_t *mac);

class IPStack
{
private:
    static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

    EventGroupHandle_t eg;
    bool connected;

public:
    IPStack(const char *ssid, const char *pw, EventGroupHandle_t event_group);
};

#endif