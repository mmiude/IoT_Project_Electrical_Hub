idf_component_register(SRC_DIRS "." "ipstack" "uart"
                       PRIV_INCLUDE_DIRS "." "ipstack" "uart"
                       PRIV_REQUIRES vfs nvs_flash spi_flash esp_wifi esp_http_client esp_websocket_client esp-tls esp_driver_uart esp_driver_gpio driver)
