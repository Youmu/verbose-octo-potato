idf_component_register(SRCS "main.c" "serial_interface.cpp" "esp_potato_client.cpp" "wifi_controller.cpp"
                    REQUIRES esp_driver_uart esp_driver_gpio esp_wifi esp_netif esp_event nvs_flash bt
                    INCLUDE_DIRS ".")
