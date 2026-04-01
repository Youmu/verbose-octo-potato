#include <string>
#include "esp_log.h"
#include "serial_interface.hpp"

static const char *TAG = "UART_IF";


espclient::SerialInterface *pSi;

extern "C" void potato_app(){
    pSi = new espclient::SerialInterface(1, 115200);
    ESP_LOGI(TAG, "Potato App started");
    pSi->SetOnReceive(
        [](std::string msg){
            ESP_LOGI(TAG, "Recv str: %s", msg.c_str());
            pSi->SendMessage("Echo: "+ msg);
        }
    );
    pSi->Start();
}
