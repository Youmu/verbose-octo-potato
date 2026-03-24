#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace mms_monitor {

using SendMessageCb = std::function<void(const std::string& message)>;
using SmsCb = std::function<bool(const std::string& sender, const std::string& body)>;
using NewMsgCb = std::function<void()>;

class AtController {
 public:
  enum class SmsStatus {
    kRecUnread = 0,
    kRecRead = 1,
    kStoUnsent = 2,
    kStoSent = 3,
    kAll = 4,
  };

  AtController() = default;

  void Init();
  void ListSms(SmsStatus status);
  void SetOnSend(SendMessageCb sendCb);
  void SetOnSmsReceived(SmsCb smsCb);
  void SetOnSmsNewMsg(NewMsgCb newMsgCb);
  void ReceiveMessage(std::string message);

 private:
  void ParseMessage(std::string_view message);

  enum class State {
    kInit,
    kCmgl,
  };

  SendMessageCb send_cb_{nullptr};
  SmsCb sms_cb_{nullptr};
  NewMsgCb new_msg_cb_{nullptr};
  State state_{State::kInit};
  int last_cmgl_index_{-1};
  int last_cmgl_message_status_{-1};
  std::string last_cmgl_body_;
};

}  // namespace mms_monitor
