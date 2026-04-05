#include <string>
#include "esp_log.h"
#include "nvs_flash.h"
#include "serial_interface.hpp"
#include "wifi_controller.hpp"
#include "time_sync.h"
#include "https_client.hpp"

static const char *TAG = "UART_IF";

using namespace espclient;

SerialInterface *pSi;
WifiController *pWifi;
time_sync *pTimeSync;
HttpsClient *pHttpsClient;

extern "C" void InitTaskFunction(void *pvParameters){
    pTimeSync->SyncTime();
    ESP_LOGI(TAG, "Time synchronized, now sending HTTP request");
    pHttpsClient->SendRequest(
        Method::GET,
        "https://www.example.com/potato/msg",
        "",
        [](int status, std::string response){
            ESP_LOGI(TAG, "HTTP GET completed with status %d, response: %s", status, response.c_str());
            vTaskDelete(NULL);
        }
    );
}


extern "C" void potato_app(){

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pWifi = new WifiController();

    pSi = new SerialInterface(1, 115200);
    
    pTimeSync = new time_sync();

    pHttpsClient = new HttpsClient();

    ESP_LOGI(TAG, "Potato App started");
    pWifi->SetOnConnected(
        [](){
            ESP_LOGI(TAG, "WiFi connected callback");
            
            BaseType_t created = xTaskCreate(
                InitTaskFunction,
                "initializer",
                8192,
                nullptr,
                5,
                nullptr);

            if (created != pdPASS) {
                ESP_LOGE(TAG, "Failed to create serial reader task");
                return;
            }
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
