#pragma once
#include <string>
#include <functional>
#include "esp_wifi.h"
#include "esp_event.h"

namespace espclient {

class WifiController {
 public:
  WifiController();
  ~WifiController();

  WifiController(const WifiController&) = delete;
  WifiController& operator=(const WifiController&) = delete;

  // Connect to a WiFi hotspot
  void ConnectToHotspot(const std::string& ssid, const std::string& password);
  
  // Disconnect from WiFi
  void Disconnect();
  
  // Check if currently connected
  bool IsConnected() const;
  
  // Set callback for connection status changes
  void SetOnConnected(std::function<void()> on_connected);
  void SetOnDisconnected(std::function<void()> on_disconnected);

 private:
  bool is_connected_{false};
  int retry_count_{0};
  std::function<void()> on_connected_;
  std::function<void()> on_disconnected_;
  
  // WiFi event handler
  static void WifiEventHandler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
};

}  // namespace espclient
