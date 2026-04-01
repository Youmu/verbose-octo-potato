#pragma once

#include <array>
#include <string>

namespace mms_monitor {

class PotatoClient {
 public:
  PotatoClient(std::string potato_ep, std::string key);

  void PushMessage(std::string timestamp, std::string sender, std::string msg);
  void SetAuthToken(std::string token);

 private:
  std::string potato_ep_;
  std::string key_base64_;
  std::string auth_token_;
  std::array<unsigned char, 32> key_bytes_{};
};

}  // namespace mms_monitor
