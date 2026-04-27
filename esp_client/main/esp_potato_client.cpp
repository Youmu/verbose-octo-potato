#include <string>
#include <format>
#include <condition_variable>
#include <mutex>
#include "esp_log.h"
#include "nvs_flash.h"
#include "serial_interface.hpp"
#include "wifi_controller.hpp"
#include "time_sync.h"
#include "https_client.hpp"
#include "AtController.hpp"
#include "RtcController.hpp"
#include "PotatoRequestBuilder.hpp"
#include "esp_timer.h"
#include "psa/crypto.h"
#include "mbedtls/base64.h"
#include "sdkconfig.h"
#include "time.h"

static const char *TAG = "POTATO_CLIENT";

using namespace espclient;

SerialInterface *pSi;
WifiController *pWifi;
time_sync *pTimeSync;
HttpsClient *pHttpsClient;
AtController *pAtController;
PotatoRequestBuilder *pRequestBuilder;
RtcController *pRtcController;

uint64_t timer_counter;

extern "C" void InitTaskFunction(void *pvParameters){
    std::mutex mtx;
    std::condition_variable cv;

    if(!pRtcController->IsTimeValid() && pTimeSync->SyncTime()) {
        // If time was not valid but we successfully synced it, update the RTC with the new time
        struct tm time_info;
        time_t now;
        time(&now);
        localtime_r(&now, &time_info);
        pRtcController->SetTime(
            time_info.tm_year + 1900,
            time_info.tm_mon + 1,
            time_info.tm_mday,
            time_info.tm_hour,
            time_info.tm_min,
            time_info.tm_sec,
            (time_info.tm_wday == 0) ? 7 : time_info.tm_wday // Convert Sunday from 0 to 7
        );
    }
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
            .callback = [&](int status, std::string response){
                ESP_LOGI(TAG, "HTTP POST completed with status %d, response: %s", status, response.c_str());
                std::unique_lock<std::mutex> lock(mtx);
                cv.notify_all();
            }
        }
    );
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock); // Wait indefinitely, or you can add a timeout if you want to exit after some time
    }

    // Initialize AT Controller callbacks

    pAtController->SetOnSmsReceived([](const std::string &sender, const std::string &body) -> bool {
        ESP_LOGI(TAG, "SMS from %s: %s", sender.c_str(), body.c_str());
        std::string request_payload = pRequestBuilder->BuildRequest(sender, body);
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
        return true; // Delete the message
    });
    pAtController->SetOnSmsNewMsg([]() {
        ESP_LOGI(TAG, "New SMS received");
        pAtController->ListSms(SmsStatus::REC_UNREAD);
    });

    pSi->Start();

    pAtController->Init();
    timer_counter = 0;
    esp_timer_create_args_t list_msg_timer = {};
    list_msg_timer.arg = &timer_counter;
    list_msg_timer.callback = [](void* p){
        uint64_t &counter = *reinterpret_cast<uint64_t*>(p);
        pAtController->ListSms(SmsStatus::REC_UNREAD);
        counter++;
        if(counter >= 12 * 60 * 2){
            counter = 0;
            pTimeSync->SyncTime();
        }
    };
    list_msg_timer.arg = pAtController;
    esp_timer_handle_t nvs_update_timer;
    ESP_ERROR_CHECK(esp_timer_create(&list_msg_timer, &nvs_update_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, 30000000)); // Every 30 seconds

    vTaskDelete(nullptr);
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

    pRtcController = new RtcController();
    pRtcController->ReadTime(true); // Read time from RTC and update system time

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

    pWifi->ConnectToHotspot(CONFIG_POTATO_WIFI_SSID, CONFIG_POTATO_WIFI_PASSWORD);
    
    // Set up AT Controller
    pAtController->SetOnSend([](const std::string &msg) {
        ESP_LOGI(TAG, "Sending AT command: %s", msg.c_str());
        pSi->SendMessage(msg);
    });

    pSi->SetOnReceive(
        [](std::string msg){
            ESP_LOGI(TAG, "Recv str: %s", msg.c_str());
            pAtController->ReceiveMessage(msg);
        }
    );
}
