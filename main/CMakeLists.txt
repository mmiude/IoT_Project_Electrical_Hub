idf_component_register(SRC_DIRS "." "zigbee" "ipstack" "interfaces" "controller" "storage" "leds" "watchdog" "lib/jsmn"
                       PRIV_INCLUDE_DIRS "." "zigbee" "ipstack" "interfaces" "controller" "storage" "leds" "watchdog" "lib/jsmn"
                       PRIV_REQUIRES vfs nvs_flash spi_flash esp_wifi esp_http_client esp-tls esp_driver_uart esp_driver_gpio jsmn)