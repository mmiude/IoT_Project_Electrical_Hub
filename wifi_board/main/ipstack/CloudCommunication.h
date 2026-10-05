#ifndef CLOUD_COMMUNICATION_H
#define CLOUD_COMMUNICATION_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "jwt.h"
#include "IPStack.h"
#include "HubEnums.h"

#include <vector>

#include "network_info.h"
#include "EventBits.h"

#define MINUTE_TO_MS 60 * 1000


class CloudCommunication
{
private:
    IPStack *ipstack;
    EventGroupHandle_t wifi_eg;

    QueueHandle_t cloud_q;
    QueueHandle_t controller_q;

    TimerHandle_t elec_price_req_timer_h;
    TimerHandle_t send_wifi_board_info_timer_h;

    TaskHandle_t cloud_task_handle;

    char efuse_mac[32] = {0};

    std::map<std::string, std::string> auth_headers = {};


    std::string ws_url;


    static void elec_price_req_timer_cb(TimerHandle_t xTimer);
    static void send_wifi_board_info_timer_cb(TimerHandle_t xTimer);

    void validate_hub();
    // void read_data();
    void send_data();
    void get_electricity_price(std::vector<float> &price_vec);
    void connect_websocket();
    void parse_websocket_data();
    void send_wifi_status(bool online);
    void send_hub_id();
    
    static void cloud_task(void *param);


public:
    CloudCommunication(IPStack *_ipstack, EventGroupHandle_t _wifi_eg, 
        QueueHandle_t _cloud_q, QueueHandle_t _controller_q
    );
    ~CloudCommunication();
};

#endif