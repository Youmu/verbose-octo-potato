#include <string>
#include "esp_log.h"
#include "nvs_flash.h"
#include "serial_interface.hpp"
#include "wifi_controller.hpp"
static const char *TAG = "UART_IF";

using namespace espclient;

SerialInterface *pSi;
WifiController *pWifi;

extern "C" void potato_app(){

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pWifi = new espclient::WifiController();

    pSi = new espclient::SerialInterface(1, 115200);
    ESP_LOGI(TAG, "Potato App started");
    pWifi->SetOnConnected(
        [](){
            ESP_LOGI(TAG, "WiFi connected callback");
        }
    );
    pWifi->SetOnDisconnected(
        [](){
            ESP_LOGI(TAG, "WiFi disconnected callback");
        }
    );
    pWifi->ConnectToHotspot("TooYoung", "123456789");
    
    pSi->SetOnReceive(
        [](std::string msg){
            ESP_LOGI(TAG, "Recv str: %s", msg.c_str());
            pSi->SendMessage("Echo: "+ msg);
        }
    );
    pSi->Start();
}
