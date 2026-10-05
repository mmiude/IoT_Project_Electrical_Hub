#include "IPStack.h"

static const char *TAG = "IPStack";
static int s_retry_num = 0;

bool get_efuse_mac(uint8_t *mac)
{
    return esp_efuse_mac_get_default(mac) == ESP_OK;
}

IPStack::IPStack(EventGroupHandle_t event_group, QueueHandle_t _wifi_q)
: eg(event_group), wifi_q(_wifi_q), storage("wifi_storage") /*, connected(false)*/
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    instance_any_id = nullptr;
    instance_got_ip = nullptr;

    ws_q = xQueueCreate(20, sizeof(t_websocket_data));
    // storage.erase_all();

    xTaskCreate(wifi_task, "WIFI_TASK", 4096, static_cast<void*>(this),
        tskIDLE_PRIORITY + 2, &wifi_task_handle);
}

IPStack::~IPStack()
{
    if (ws_client) {
        esp_websocket_client_stop(ws_client);
        esp_websocket_client_destroy(ws_client);
    }
    if (ws_q) {
        vQueueDelete(ws_q);
    }
}

void IPStack::wifi_task(void *param)
{
    auto ipstack = static_cast<IPStack*>(param);
    auto storage = ipstack->storage;

    bool got_ssid = false;
    bool got_pw = false;
    std::string ssid;
    std::string pw;
    controller_data ctrl_data = {};
    while (true) {
        EventBits_t wifi_bits = ipstack->get_wifi_bits(pdMS_TO_TICKS(500));

        if (wifi_bits & WIFI_CONNECTED_BIT) {
            if (xQueueReceive(ipstack->wifi_q, &ctrl_data, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (ctrl_data.type == DATA_TYPE_WIFI_SSID) {
                    ssid = ctrl_data.data.c_value;
                    got_ssid = true;
                }
                if (ctrl_data.type == DATA_TYPE_WIFI_PW) {
                    pw = ctrl_data.data.c_value;
                    got_pw = true;
                }
            }
            if (got_ssid && got_pw) {
                ESP_LOGI(TAG, "Got new wifi credentials reconnecting...");
                ipstack->disconnect_wifi();
            }
        } else {
            // When disconnected, fetch credentials from NVS/UART and connect
            if (ssid.empty() || pw.empty()) {
                esp_err_t ssid_err = storage.read_string(ipstack->ssid_key, ssid);
                esp_err_t pw_err = storage.read_string(ipstack->pw_key, pw);
                if (ssid_err != ERR_OK || pw_err != ERR_OK) {
                    ESP_LOGI(TAG, "No wifi credentials in nvs. Waiting from uart...");
                    if (xQueueReceive(ipstack->wifi_q, &ctrl_data, portMAX_DELAY) == pdTRUE) {
                        if (ctrl_data.type == DATA_TYPE_WIFI_SSID) {
                            ssid = ctrl_data.data.c_value;
                            got_ssid = true;
                        }
                        if (ctrl_data.type == DATA_TYPE_WIFI_PW) {
                            pw = ctrl_data.data.c_value;
                            got_pw = true;
                        }
                    }
                    if (got_ssid && got_pw) {
                        ESP_LOGI(TAG, "Got wifi ssid and pw");
                    }
                }
            } else {
                bool connected = ipstack->connect_wifi(ssid.c_str(), pw.c_str());
                if (connected) {
                    ESP_LOGI(TAG, "Wifi connected");
                    storage.write_string(ipstack->ssid_key, ssid);
                    storage.write_string(ipstack->pw_key, pw);
                } else {
                    ESP_LOGE(TAG, "Wifi connect failed");
                    storage.erase_key(ipstack->ssid_key);
                    storage.erase_key(ipstack->pw_key);
                }
                ssid = "";
                pw = "";
                got_ssid = false;
                got_pw = false;
            }
        }
    }
}

bool IPStack::get_wifi_credentials_from_uart(std::string &str, TickType_t delay)
{
    controller_data ctrl_data = {};
    if (xQueueReceive(wifi_q, &ctrl_data, 0)) {
        str = ctrl_data.data.c_value;
        return true;
    }
    return false;
}


EventBits_t IPStack::get_wifi_bits(TickType_t delay)
{
    // ESP_LOGI(TAG, "Waiting for wifi bits");
    EventBits_t bits = xEventGroupWaitBits(eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        delay);
    // ESP_LOGI(TAG, "Got wifi bits");


    return bits;
}

bool IPStack::connect_wifi(const char *ssid, const char *pw)
{
    // Reset retry counter for each new connection attempt
    s_retry_num = 0;

    ESP_LOGI(TAG, "Connecting to: %s", ssid);

    // Clear stale bits before waiting
    xEventGroupClearBits(eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        static_cast<void *>(this),
        &instance_any_id)
    );
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        static_cast<void *>(this),
        &instance_got_ip)
    );
    
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pw, sizeof(wifi_config.sta.password));

    // DO NOT hardcode channel = 1 unless your AP is fixed on Channel 1
    // wifi_config.sta.channel = 1; 

    // Explicitly set auth threshold to avoid scanning overhead
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t wifi_bits = get_wifi_bits(portMAX_DELAY);
    if (wifi_bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", ssid);
        return true;
    }
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", ssid);
    disconnect_wifi();

    return false;
}

void IPStack::disconnect_wifi()
{

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());

    if (instance_any_id != nullptr) {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            instance_any_id)
        );
        instance_any_id = nullptr;
    }
    if (instance_got_ip != nullptr) {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            instance_got_ip)
        );
        instance_got_ip = nullptr;
    }

    xEventGroupSetBits(eg, WIFI_FAIL_BIT);
    xEventGroupClearBits(eg, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG, "Wifi disconnected");
}

void IPStack::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    auto ipstack = static_cast<IPStack*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(ipstack->eg, WIFI_FAIL_BIT);
            xEventGroupClearBits(ipstack->eg, WIFI_CONNECTED_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(ipstack->eg, WIFI_CONNECTED_BIT | ON_WIFI_CONNECT_BIT);
    }
}

esp_err_t IPStack::http_event_handler(esp_http_client_event_t *evt)
{
    static int output_len = 0; // Stores number of bytes read into caller buffer

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            output_len = 0; // Reset length counter for new request
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            if (evt->user_data != nullptr) {
                // Clear caller buffer on first data chunk
                if (output_len == 0) {
                    memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
                }

                // Process incoming body chunk
                int copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len > 0) {
                    memcpy(static_cast<char *>(evt->user_data) + output_len, evt->data, copy_len);
                    output_len += copy_len;
                    // Ensure null termination
                    static_cast<char *>(evt->user_data)[output_len] = '\0';
                }
            } else {
                ESP_LOGE(TAG, "No user_data response buffer provided for HTTP request!");
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            break;

        case HTTP_EVENT_DISCONNECTED: {
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            output_len = 0;
            break;
        }

        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;

        default:
            break;
    }
    return ESP_OK;
}

void IPStack::websocket_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto ipstack = static_cast<IPStack *>(arg);
    auto data = static_cast<esp_websocket_event_data_t *>(event_data);

    switch (event_id)
    {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            xEventGroupSetBits(ipstack->eg, WEBSOCKET_CONNECTED_BIT);
            xEventGroupClearBits(ipstack->eg, WEBSOCKET_ERROR_BIT);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            xEventGroupClearBits(ipstack->eg, WEBSOCKET_CONNECTED_BIT);
            xEventGroupClearBits(ipstack->eg, WEBSOCKET_ERROR_BIT);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->data_len == 0) break;

            ESP_LOGW(TAG, "WEBSOCKET_EVENT_DATA. Len: %d", data->data_len);
            if (data->data_ptr != nullptr && data->data_len > 0) {
                t_websocket_data ws_data = {};
                ws_data.op_code = data->op_code;

                // Copy up to buffer capacity and null-terminate
                size_t copy_len = std::min((size_t)data->data_len, sizeof(ws_data.payload) - 1);
                memcpy(ws_data.payload, data->data_ptr, copy_len);
                ws_data.payload[copy_len] = '\0';

                if (xQueueSendToBack(ipstack->ws_q, &ws_data, 0) == pdTRUE) {
                    ESP_LOGI(TAG, "Websocket data passed successfully");
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
            xEventGroupSetBits(ipstack->eg, WEBSOCKET_ERROR_BIT);
            xEventGroupClearBits(ipstack->eg, WEBSOCKET_CONNECTED_BIT);
            break;
        
        default:
            break;
    }
}

bool IPStack::call_http_request(t_http_request req)
{
    bool success = false;

    if (req.method == HTTP_METHOD_POST) {
        esp_http_client_set_post_field(*req.client, req.body_data, strlen(req.body_data));
    }

    esp_err_t err = esp_http_client_perform(*req.client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(*req.client);
        ESP_LOGI(TAG, "HTTP %d Status = %d, content_length = %" PRId64,
                (int)req.method,
                status_code,
                esp_http_client_get_content_length(*req.client));
        success = status_code >= 200 && status_code < 300;
    } else {
        ESP_LOGE(TAG, "HTTP %d request failed: %s", (int)req.method, esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "%s", req.response_buff);
    ESP_ERROR_CHECK(esp_http_client_cleanup(*req.client));
    return success;
}

bool IPStack::http_request(const char *hostname, int port, char *response_buff,
                    const char *path, const char *query, const char *body_data,
                    esp_http_client_method_t method, std::map<std::string, std::string> headers)
{
    esp_http_client_config_t config = {};
    config.host = hostname;
    config.port = port,
    config.path = path;
    config.method = method;
    config.query = query;
    config.event_handler = http_event_handler;
    config.user_data = response_buff;
    config.disable_auto_redirect = true;

    ESP_LOGI(TAG, "HTTP %d %s", (int)method, hostname);
    esp_http_client_handle_t client = esp_http_client_init(&config);

    for (auto const& [key, val] : headers) {
        esp_http_client_set_header(client, key.c_str(), val.c_str());
    }

    t_http_request req = {
        .client = &client,
        .method = method,
        .body_data = body_data,
        .response_buff = response_buff
    };
    return call_http_request(req);
}

bool IPStack::http_request(const char *url, char *response_buff, const char *body_data,
                    const char *tls_cert, esp_http_client_method_t method,
                    std::map<std::string, std::string> headers)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = method;
    config.event_handler = http_event_handler;
    config.user_data = response_buff;
    config.disable_auto_redirect = true;

    if (tls_cert == nullptr || tls_cert[0] == '\0') {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    } else {
        config.cert_pem = tls_cert;
    }

    ESP_LOGI(TAG, "HTTP %d %s", (int)method, url);
    esp_http_client_handle_t client = esp_http_client_init(&config);

    for (auto const& [key, val] : headers) {
        esp_http_client_set_header(client, key.c_str(), val.c_str());
    }

    t_http_request req = {
        .client = &client,
        .method = method,
        .body_data = body_data,
        .response_buff = response_buff
    };
    return call_http_request(req);
}

esp_err_t IPStack::init_websocket(const char *uri)
{
    // Clean up previous client and close open sockets before starting a new one
    if (ws_client != nullptr) {
        esp_websocket_client_stop(ws_client);
        esp_websocket_client_destroy(ws_client);
        ws_client = nullptr;
    }

    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = uri;
    ws_cfg.network_timeout_ms = WEBSOCKET_NETWORK_TIMEOUT_MS; // Explicitly set network timeout
    ws_cfg.ping_interval_sec = 10;
    ws_cfg.pingpong_timeout_sec = 5; 

    ws_client = esp_websocket_client_init(&ws_cfg);
    if (ws_client == nullptr) return ESP_ERR_NO_MEM;

    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, static_cast<void *>(this));
    return esp_websocket_client_start(ws_client);
}

esp_err_t IPStack::deinit_websocket()
{
    if (ws_client == nullptr) {
        return ESP_OK;
    }

    esp_websocket_unregister_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler);
    esp_err_t stop_err = esp_websocket_client_stop(ws_client);
    esp_err_t destroy_err = esp_websocket_client_destroy(ws_client);

    ws_client = nullptr;

    return (stop_err != ESP_OK) ? stop_err : destroy_err;
}

bool IPStack::get_websocket_data(t_websocket_data *ws_data, int timeout_ms)
{
    return xQueueReceive(ws_q, ws_data, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}