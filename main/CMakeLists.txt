idf_component_register(SRC_DIRS "." "zigbee" "ipstack"
                       PRIV_INCLUDE_DIRS "." "zigbee" "ipstack"
                       PRIV_REQUIRES vfs nvs_flash spi_flash esp_wifi esp_http_client esp-tls esp_driver_uart)