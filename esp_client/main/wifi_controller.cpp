#include "wifi_controller.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WifiController";
static EventGroupHandle_t s_wifi_event_group = NULL;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;
static const int MAX_RETRY_COUNT = 5;

namespace espclient {

WifiController::WifiController() : is_connected_(false) {
  // Initialize network interface
  ESP_ERROR_CHECK(esp_netif_init());
  
  // Create default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  
  // Create WiFi STA instance
  esp_netif_create_default_wifi_sta();
  
  // Initialize WiFi driver
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  
  // Create event group for WiFi events
  if (s_wifi_event_group == NULL) {
    s_wifi_event_group = xEventGroupCreate();
  }
  
  // Register WiFi event handler
  /*
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &WifiController::WifiEventHandler,
                                             this));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &WifiController::WifiEventHandler,
                                             this));
  */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &WifiController::WifiEventHandler,
                                             this, 
                                             &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &WifiController::WifiEventHandler,
                                             this, 
                                             &instance_got_ip));
  ESP_LOGI(TAG, "WiFi controller initialized");
}

WifiController::~WifiController() {
  // Unregister event handlers
  esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &WifiController::WifiEventHandler);
  esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &WifiController::WifiEventHandler);
  
  // Deinitialize WiFi
  ESP_ERROR_CHECK(esp_wifi_deinit());
  ESP_ERROR_CHECK(esp_event_loop_delete_default());
  esp_netif_deinit();
  
  ESP_LOGI(TAG, "WiFi controller destroyed");
}

void WifiController::ConnectToHotspot(const std::string& ssid,
                                      const std::string& password) {
  ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid.c_str());
  
  // Set WiFi mode to STA (Station)
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  
  // Configure WiFi credentials
  wifi_config_t wifi_config = {};
  std::copy(ssid.begin(), ssid.end(), wifi_config.sta.ssid);
  std::copy(password.begin(), password.end(), wifi_config.sta.password);
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
  
  // Set SSID length explicitly
  wifi_config.sta.ssid[ssid.length()] = '\0';
  wifi_config.sta.password[password.length()] = '\0';
  
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

}

void WifiController::Disconnect() {
  if (is_connected_) {
    ESP_LOGI(TAG, "Disconnecting from WiFi");
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    is_connected_ = false;
  }
}

bool WifiController::IsConnected() const {
  return is_connected_;
}

void WifiController::SetOnConnected(std::function<void()> on_connected) {
  on_connected_ = on_connected;
}

void WifiController::SetOnDisconnected(std::function<void()> on_disconnected) {
  on_disconnected_ = on_disconnected;
}

void WifiController::WifiEventHandler(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data) {
  WifiController* self = static_cast<WifiController*>(arg);
  
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "WiFi started, attempting to connect");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    ESP_LOGI(TAG, "WiFi connected");
    self->retry_count_ = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "WiFi disconnected");
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    self->is_connected_ = false;
    self->retry_count_++;
    if (self->on_disconnected_) {
      self->on_disconnected_();
    }
    if(MAX_RETRY_COUNT >= 0 && self->retry_count_ > MAX_RETRY_COUNT) {
      ESP_LOGE(TAG, "Exceeded maximum WiFi retry count");
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      return;
    }
    // Attempt to reconnect
    ESP_LOGI(TAG, "Attempting to reconnect to WiFi");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
    ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    self->is_connected_ = true;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    if (self->on_connected_) {
      self->on_connected_();
    }
  }
}

}  // namespace espclient
