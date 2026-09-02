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

#include "esp_http_client.h"
#include "esp_tls.h"

#include "network_info.h"
#include "map"
#include "string"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1 
#define DEVICE_SIGN_READY   BIT2 

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

#define API_HOSTNAME "192.168.101.105"
#define API_PORT 3000




bool get_efuse_mac(uint8_t *mac);

class IPStack
{
private:
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    EventGroupHandle_t eg;
    // bool connected;
public:
    IPStack(const char *ssid, const char *pw, EventGroupHandle_t event_group);

    bool http_request(const char *hostname, int port, char *response_buff,
                    const char *path = "/", const char *query = "", const char *body_data = "",
                    esp_http_client_method_t method = HTTP_METHOD_GET,
                    std::map<std::string, std::string> headers = {});
    
    // bool operator()();
};

#endif