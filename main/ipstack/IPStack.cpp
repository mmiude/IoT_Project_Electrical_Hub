#include "IPStack.h"

static const char *TAG = "IPStack";
static int s_retry_num = 0;

bool get_efuse_mac(uint8_t *mac)
{
    return esp_efuse_mac_get_default(mac) == ESP_OK;
}

IPStack::IPStack(EventGroupHandle_t event_group)
: eg(event_group)/*, connected(false)*/
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    instance_any_id = nullptr;
    instance_got_ip = nullptr;
}

bool IPStack::connect_wifi(const char *ssid, const char *pw)
{
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
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                    ssid, pw);
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                    ssid, pw);
        return false;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return false;
    }
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
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(ipstack->eg, WIFI_CONNECTED_BIT);
    }
}

esp_err_t IPStack::http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        // case HTTP_EVENT_ON_HEADERS_COMPLETE:
        //     ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADERS_COMPLETE");
        //     break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             * Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             * However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        // FIX 1: Cast void* to char* so the compiler knows how many bytes to offset
                        memcpy((char *)evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
#if CONFIG_EXAMPLE_ENABLE_RESPONSE_BUFFER_DUMP
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
#endif
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
            
        case HTTP_EVENT_DISCONNECTED: { // FIX 2: Added opening brace for scope isolation
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        } // FIX 2: Added closing brace

        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
            
        // case HTTP_EVENT_ON_STATUS_CODE: // FIX 3: Handled missing enum warning
        //     ESP_LOGD(TAG, "HTTP_EVENT_ON_STATUS_CODE");
        //     break;
            
        default:
            break;
    }
    return ESP_OK;
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

    bool success = false;

    ESP_LOGI(TAG, "HTTP %d %s", (int)method, hostname);
    esp_http_client_handle_t client = esp_http_client_init(&config);

    for (auto const& [key, val] : headers) {
        esp_http_client_set_header(client, key.c_str(), val.c_str());
    }

    switch (method)
    {
    case HTTP_METHOD_GET:
        break;
    case HTTP_METHOD_POST:
        // esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body_data, strlen(body_data));
        break;
    default:
        break;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %d Status = %d, content_length = %" PRId64,
                (int)method,
                status_code,
                esp_http_client_get_content_length(client));
        success = status_code >= 200 && status_code < 300;
    } else {
        ESP_LOGE(TAG, "HTTP %d request failed: %s", (int)method, esp_err_to_name(err));
    }
    ESP_LOG_BUFFER_HEX(TAG, response_buff, strlen(response_buff));
    ESP_ERROR_CHECK(esp_http_client_cleanup(client));
    return success;
}

// bool IPStack::operator()() { return connected; }