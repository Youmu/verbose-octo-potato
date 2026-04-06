#include <string>
#include <format>
#include "esp_log.h"
#include "nvs_flash.h"
#include "serial_interface.hpp"
#include "wifi_controller.hpp"
#include "time_sync.h"
#include "https_client.hpp"
#include "AtController.hpp"
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

extern "C" void InitTaskFunction(void *pvParameters){
    pTimeSync->SyncTime();
    ESP_LOGI(TAG, "Time synchronized, now sending HTTP request");


    std::string key_64 = CONFIG_POTATO_MSG_ENCRYPT_KEY;
    uint8_t key_aes[32];
    size_t key_len = 32;

    uint8_t iv[16];
    memset(iv, 0xEE, 16);

    int ret = mbedtls_base64_decode(key_aes, sizeof(key_aes), &key_len,
                                    reinterpret_cast<const unsigned char*>(key_64.c_str()), key_64.size());
    std::string message = "Hello, this is a secret message!";

    auto psa_status = psa_crypto_init();
    psa_algorithm_t alg = PSA_ALG_CBC_PKCS7;

    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;

    psa_set_key_type(&key_attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&key_attr, 256);
    psa_set_key_usage_flags(&key_attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&key_attr, alg);

    psa_status = psa_import_key(&key_attr, key_aes, sizeof(key_aes), &key_id);

    if(psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Key import failed: %d", psa_status);
        vTaskDelete(NULL);
        return;
    }

    uint8_t output[256];
    size_t output_len = 0;  

    psa_status = psa_cipher_encrypt(
        key_id,
        alg,
        reinterpret_cast<const uint8_t*>(message.c_str()), message.size(),
        output, sizeof(output), &output_len
    );

    if(psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Encryption failed: %d", psa_status);
        psa_destroy_key(key_id);
        vTaskDelete(NULL);
        return;
    }

    unsigned char base64_output[512];
    size_t base64_output_len = 0;
    ret = mbedtls_base64_encode(base64_output, sizeof(base64_output), &base64_output_len, output, output_len);
    if(ret != 0) {
        ESP_LOGE(TAG, "Base64 encoding failed: %d", ret);
        psa_destroy_key(key_id);
        vTaskDelete(NULL);
        return;
    }


    psa_destroy_key(key_id);

    std::string msg = std::format(
        "{{\"TimeStamp\":\"2026-04-06T13:29:03.123Z\",\"From\":\"13912345678\",\"Data\":\"{}\"}}", 
        std::string(reinterpret_cast<char*>(base64_output), base64_output_len)
    );
    ESP_LOGI(TAG, "Encrypted message: %s", msg.c_str());
    pHttpsClient->SetAuthToken(CONFIG_POTATO_SERVER_AUTHTOKEN);
    pHttpsClient->SendRequest(
        Method::POST,
        CONFIG_POTATO_SERVER_EP,
        msg,
        [](int status, std::string response){
            ESP_LOGI(TAG, "HTTP POST completed with status %d, response: %s", status, response.c_str());
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
