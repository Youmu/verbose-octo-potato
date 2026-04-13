idf_component_register(SRCS "main.c" "serial_interface.cpp" "esp_potato_client.cpp" "wifi_controller.cpp" "time_sync.cpp" "https_client.cpp" "AtController.cpp" "PotatoRequestBuilder.cpp" "RtcController.cpp"
                    REQUIRES esp_driver_uart esp_driver_gpio esp_wifi esp_netif esp_event nvs_flash esp_http_client esp_timer mbedtls esp_driver_i2c
                    INCLUDE_DIRS ".")
