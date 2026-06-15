#ifndef DEVICE_SIGN_H
#define DEVICE_SIGN_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "jwt.h"
#include "IPStack.h"


class DeviceSign
{
private:
    IPStack *ipstack;
    EventGroupHandle_t wifi_eg;

    static void sign_task(void *param);

public:
    DeviceSign(IPStack *_ipstack, EventGroupHandle_t _wifi_eg);
};

#endif