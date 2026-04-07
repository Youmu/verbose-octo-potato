#include <string>
#include <format>
#include "esp_log.h"
#include "nvs_flash.h"
#include "serial_interface.hpp"
#include "wifi_controller.hpp"
#include "time_sync.h"
#include "https_client.hpp"
#include "AtController.hpp"
#include "PotatoRequestBuilder.hpp"
#include "esp_timer.h"
#include "psa/crypto.h"
#include "mbedtls/base64.h"
#include "sdkconfig.h"

static const char *TAG = "POTATO_CLIENT";

using namespace espclient;

SerialInterface *pSi;
WifiController *pWifi;
time_sync *pTimeSync;
HttpsClient *pHttpsClient;
AtController *pAtController;
PotatoRequestBuilder *pRequestBuilder;

extern "C" void InitTaskFunction(void *pvParameters){
    pTimeSync->SyncTime();
    ESP_LOGI(TAG, "Time synchronized, now sending HTTP request");
    pRequestBuilder = new PotatoRequestBuilder(CONFIG_POTATO_MSG_ENCRYPT_KEY);

    std::string from = "13912345679";
    std::string message = "Hello, Potato!";
    std::string request_payload = pRequestBuilder->BuildRequest(from, message);

    ESP_LOGI(TAG, "Encrypted message: %s", request_payload.c_str());
    
    pHttpsClient->SetAuthToken(CONFIG_POTATO_SERVER_AUTHTOKEN);
    pHttpsClient->Start();
    pHttpsClient->PushRequest(
        HttpsRequest{
            .method = Method::POST,
            .uri = CONFIG_POTATO_SERVER_EP,
            .payload = request_payload,
            .callback = [](int status, std::string response){
                ESP_LOGI(TAG, "HTTP POST completed with status %d, response: %s", status, response.c_str());
            }
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

    pAtController = new AtController();

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
    
    // Set up AT Controller
    pAtController->SetOnSend([](const std::string &msg) {
        ESP_LOGI(TAG, "Sending AT command: %s", msg.c_str());
        pSi->SendMessage(msg);
    });
    pAtController->SetOnSmsReceived([](const std::string &sender, const std::string &body) -> bool {
        ESP_LOGI(TAG, "SMS from %s: %s", sender.c_str(), body.c_str());
        return true; // Delete the message
    });
    pAtController->SetOnSmsNewMsg([]() {
        ESP_LOGI(TAG, "New SMS received");
        pAtController->ListSms(SmsStatus::REC_UNREAD);
    });

    pSi->SetOnReceive(
        [](std::string msg){
            ESP_LOGI(TAG, "Recv str: %s", msg.c_str());
            pAtController->ReceiveMessage(msg);
        }
    );
    pSi->Start();
    // Initialize AT Controller
    pAtController->Init();

    esp_timer_create_args_t list_msg_timer = {};

    list_msg_timer.callback = [](void* p){
                ESP_LOGI(TAG, "Periodic timer callback: List message");
                // Here you would add code to update NVS or perform other periodic tasks
                auto atController = reinterpret_cast<AtController*>(p);
                atController->ListSms(SmsStatus::REC_UNREAD);
            };
    list_msg_timer.arg = pAtController;
    esp_timer_handle_t nvs_update_timer;
    ESP_ERROR_CHECK(esp_timer_create(&list_msg_timer, &nvs_update_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, 30000000)); // Every 30 seconds
}
