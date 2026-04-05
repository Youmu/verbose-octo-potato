#pragma once
#include <string>
#include <functional>
#include <vector>

namespace espclient {

enum class SmsStatus {
    REC_UNREAD = 0,
    REC_READ = 1,
    STO_UNSENT = 2,
    STO_SENT = 3,
    ALL = 4
};

class AtController {
public:
    using SendMessageCb = std::function<void(const std::string &message)>;
    using SmsCb = std::function<bool(const std::string &sender, const std::string &body)>;
    using NewMsgCb = std::function<void()>;

    AtController();
    ~AtController();

    void Init();
    void ListSms(SmsStatus status);
    void SetOnSend(SendMessageCb sendCb);
    void SetOnSmsReceived(SmsCb smsCb);
    void SetOnSmsNewMsg(NewMsgCb newMsgCb);
    void ReceiveMessage(const std::string &message);

private:
    enum class State {
        Init,
        CMGL
    };

    State current_state_;
    SendMessageCb send_cb_;
    SmsCb sms_cb_;
    NewMsgCb new_msg_cb_;

    int current_index_;
    bool waiting_pdu_;

    void SendCommand(const std::string &cmd);
    void ParseMessage(const std::string &message);
    bool DecodePDU(const std::string &pdu, std::string &sender, std::string &body);
    bool ParseCmglHeader(const std::string &line, int &index_out) const;
    bool IsHexLine(const std::string &line) const;
    bool HexToBytes(const std::string &hex, std::vector<uint8_t> &bytes) const;
    std::string DecodeSemiOctetAddress(
        const std::vector<uint8_t> &bytes,
        size_t start,
        size_t digit_count,
        uint8_t toa) const;
    std::string DecodeUcs2ToUtf8(const std::vector<uint8_t> &ud) const;
    std::string DecodeGsm7ToUtf8(
        const std::vector<uint8_t> &ud,
        size_t septet_count,
        int bit_offset) const;
    std::string Gsm7ToUtf8Char(uint8_t septet) const;
};

}  // namespace espclient