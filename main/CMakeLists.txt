idf_component_register(SRCS "main.cpp" "IPStack.cpp" "jwt.cpp" "device_sign.cpp"
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash spi_flash esp_wifi esp_http_client esp-tls esp_driver_uart)

