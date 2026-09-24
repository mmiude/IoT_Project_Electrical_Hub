#ifndef CLOUD_COMMUNICATION_H
#define CLOUD_COMMUNICATION_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "jwt.h"
#include "IPStack.h"
#include "HubControllerEnums.h"

#include <vector>

#include "network_info.h"

#define JSMN_STATIC
#include "jsmn.h"

#define THINGSPEAK_CERT "-----BEGIN CERTIFICATE-----\n\
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n\
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n\
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n\
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n\
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n\
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n\
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n\
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n\
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n\
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n\
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n\
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n\
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n\
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n\
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n\
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n\
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n\
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n\
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n\
MrY=\n\
-----END CERTIFICATE-----\n"

#define THINGSPEACK_TB_URL "https://api.thingspeak.com/talkbacks/%d/commands/execute.json"

// enum class Commands {
//     TOGGLE_PLUG,
//     PLUG_ON,
//     PLUG_OFF,
//     OPEN_NETWORK
// };

// typedef struct {
//     Commands command;
//     uint64_t device_id;
// } HubCommand;

#define JSMN_TOKENS_SIZE 20
#define MINUTE_TO_MS 60 * 1000

class CloudCommunication
{
private:
    IPStack *ipstack;
    EventGroupHandle_t wifi_eg;

    // QueueHandle_t tb_command_q;
    QueueHandle_t cloud_control_q;
    QueueHandle_t cloud_q;
    QueueHandle_t controller_q;

    TimerHandle_t elec_price_req_timer_h;
    TimerHandle_t cloud_comm_timer_h;

    // uint8_t efuse_mac[6];
    char efuse_mac[32] = {0};
    // char hub_jwt[512] = {0};
    // int jwt_error = -1;

    std::map<std::string, std::string> auth_headers = {};
    // const std::map<std::string, std::string> tb_headers = {
    //     { "Host", "api.thingspeak.com" },
    //     { "Content-Type", "application/x-www-form-urlencoded" },
    //     { "Accept", "*/*" }
    // };

    // std::string tb_url;
    // std::string read_http_body;

    std::string ws_url;

    // bool generate_hub_jwt(char *buffer, size_t size);

    static void elec_price_req_timer_cb(TimerHandle_t xTimer);
    static void send_data_timer_cb(TimerHandle_t xTimer);

    void validate_hub();
    // void read_data();
    void send_data();
    void get_electricity_price(std::vector<float> &price_vec);
    void connect_websocket();
    void parse_websocket_data();
    
    static void cloud_task(void *param);

    // static void sign_task(void *param);
    // static void read_and_send_task(void *param);
    // static void get_electricity_price_task(void *param);
    // bool parse_talkback_response_json(const char *response, controller_data *ctrl_data);

public:
    CloudCommunication(IPStack *_ipstack, EventGroupHandle_t _wifi_eg, 
        QueueHandle_t _cloud_q, QueueHandle_t _controller_q);
};

#endif