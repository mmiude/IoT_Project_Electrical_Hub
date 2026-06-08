/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
// #include "esp_mac.h"
// #include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "network_info.h"
#include "IPStack.h"


static const char *TAG = "wifi station";


extern "C" void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    EventGroupHandle_t wifi_eg = xEventGroupCreate();
    IPStack ipstack(SSID, PW, wifi_eg);

    uint8_t mac[6];
    if (get_efuse_mac(mac)) {
        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        printf("No MAC found\n");
    }

    while (true) {
        printf("Connected yaaaay :)");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    // for (int i = 10; i >= 0; i--) {
    //     printf("Restarting in %d seconds...\n", i);
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
    // printf("Restarting now.\n");
    // fflush(stdout);
    // esp_restart();
}
