#pragma once

#include <array>
#include <string>

namespace mms_monitor {

class PotatoClient {
 public:
  PotatoClient(std::string potato_ep, std::string key);

  void PushMessage(std::string timestamp, std::string sender, std::string msg);

 private:
  std::string potato_ep_;
  std::string key_base64_;
  std::array<unsigned char, 32> key_bytes_{};
};

}  // namespace mms_monitor
