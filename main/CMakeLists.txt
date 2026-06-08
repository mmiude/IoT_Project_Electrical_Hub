idf_component_register(SRCS "main.cpp" "IPStack.cpp"
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash spi_flash esp_wifi)
