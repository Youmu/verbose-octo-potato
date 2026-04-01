idf_component_register(SRCS "main.c" "serial_interface.cpp" "esp_potato_client.cpp"
                    REQUIRES esp_driver_uart esp_driver_gpio
                    INCLUDE_DIRS ".")
