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
#include "esp_crt_bundle.h"
#include "esp_websocket_client.h"

#include "network_info.h"
#include "map"
#include "string"

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1 
#define DEVICE_SIGN_READY       BIT2 
#define ON_WIFI_CONNECT_BIT     BIT4
#define WEBSOCKET_CONNECTED_BIT BIT8
#define WEBSOCKET_ERROR_BIT     BIT9

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

// #define API_HOSTNAME "10.161.4.38"
#define API_HOSTNAME "192.168.101.105"
#define API_PORT 3000
#define WS_PORT 8080

#define WEBSOCKET_NETWORK_TIMEOUT_MS 10000

bool get_efuse_mac(uint8_t *mac);

typedef struct {
    esp_http_client_handle_t *client;
    esp_http_client_method_t method;
    const char *body_data;
    char *response_buff;
} t_http_request;

typedef struct {
    char payload[512];
    int op_code;
} t_websocket_data;

class IPStack
{
private:
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    static void websocket_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);

    bool call_http_request(t_http_request req);

    EventGroupHandle_t eg;
    QueueHandle_t ws_q;

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_websocket_client_handle_t ws_client = nullptr;

    // bool connected;
public:
    IPStack(EventGroupHandle_t event_group);
    ~IPStack();

    bool connect_wifi(const char *ssid, const char *pw);
    void disconnect_wifi();
    bool wait_for_wifi();

    bool http_request(const char *hostname, int port, char *response_buff,
                    const char *path = "/", const char *query = "", const char *body_data = "",
                    esp_http_client_method_t method = HTTP_METHOD_GET,
                    std::map<std::string, std::string> headers = {});

    bool http_request(const char *url, char *response_buff, const char *body_data = "",
                    const char *tls_cert = "", esp_http_client_method_t method = HTTP_METHOD_GET,
                    std::map<std::string, std::string> headers = {});

    esp_err_t init_websocket(const char *uri);
    bool get_websocket_data(t_websocket_data *ws_data, int timeout_ms);
    
    // bool operator()();
};

#endif