#include "AtController.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>

namespace espclient {

AtController::AtController()
    : current_state_(State::Init),
      current_index_(-1),
      waiting_pdu_(false) {}

AtController::~AtController() {}

void AtController::Init() {
    SendCommand("AT+CMGF=0");
}

void AtController::ListSms(SmsStatus status) {
    SendCommand("AT+CMGL=" + std::to_string(static_cast<int>(status)));
}

void AtController::SetOnSend(SendMessageCb sendCb) {
    send_cb_ = sendCb;
}

void AtController::SetOnSmsReceived(SmsCb smsCb) {
    sms_cb_ = smsCb;
}

void AtController::SetOnSmsNewMsg(NewMsgCb newMsgCb) {
    new_msg_cb_ = newMsgCb;
}

void AtController::ReceiveMessage(const std::string &message) {
    ParseMessage(message);
}

void AtController::SendCommand(const std::string &cmd) {
    if (send_cb_) {
        send_cb_(cmd);
    }
}

void AtController::ParseMessage(const std::string &message) {
    std::string trimmed = message;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), trimmed.end());

    if (trimmed.empty()) {
        return;
    }

    // Ignore command echoes.
    if (trimmed.rfind("at+", 0) == 0 || trimmed.rfind("AT+", 0) == 0) {
        return;
    }

    if (trimmed.rfind("+CMGL", 0) == 0) {
        int parsed_index = -1;
        if (ParseCmglHeader(trimmed, parsed_index)) {
            current_index_ = parsed_index;
            current_state_ = State::CMGL;
            waiting_pdu_ = true;
        }
        return;
    }

    if (trimmed.rfind("+CMTI", 0) == 0) {
        if (new_msg_cb_) {
            new_msg_cb_();
        }
        return;
    }

    if (trimmed == "OK") {
        current_state_ = State::Init;
        current_index_ = -1;
        waiting_pdu_ = false;
        return;
    }

    if (current_state_ != State::CMGL || !waiting_pdu_) {
        return;
    }

    if (!IsHexLine(trimmed)) {
        return;
    }

    std::string sender;
    std::string body;
    if (DecodePDU(trimmed, sender, body) && sms_cb_) {
        bool handled = sms_cb_(sender, body);
        if (handled && current_index_ >= 0) {
            SendCommand("AT+CMGD=" + std::to_string(current_index_));
        }
    }

    waiting_pdu_ = false;
    current_index_ = -1;
}

bool AtController::DecodePDU(const std::string &pdu, std::string &sender, std::string &body) {
    std::vector<uint8_t> bytes;
    if (!HexToBytes(pdu, bytes) || bytes.size() < 12) {
        return false;
    }

    size_t pos = 0;
    uint8_t smsc_len = bytes[pos++];
    if (pos + smsc_len > bytes.size()) {
        return false;
    }
    pos += smsc_len;

    if (pos + 2 > bytes.size()) {
        return false;
    }
    uint8_t first_octet = bytes[pos++];
    bool has_udhi = (first_octet & 0x40) != 0;

    uint8_t oa_digit_len = bytes[pos++];
    if (pos >= bytes.size()) {
        return false;
    }
    uint8_t toa = bytes[pos++];

    size_t oa_octets = (oa_digit_len + 1) / 2;
    if (pos + oa_octets > bytes.size()) {
        return false;
    }
    sender = DecodeSemiOctetAddress(bytes, pos, oa_digit_len, toa);
    pos += oa_octets;

    // PID + DCS + SCTS
    if (pos + 1 + 1 + 7 > bytes.size()) {
        return false;
    }
    pos += 1; // PID
    uint8_t dcs = bytes[pos++];
    pos += 7; // SCTS

    if (pos >= bytes.size()) {
        return false;
    }
    uint8_t udl = bytes[pos++];
    if (pos > bytes.size()) {
        return false;
    }
    std::vector<uint8_t> ud(bytes.begin() + static_cast<long>(pos), bytes.end());

    // DCS alphabet: 0=GSM7, 1=8-bit, 2=UCS2 for general coding group.
    uint8_t alphabet = 0;
    if ((dcs & 0xC0) == 0x00) {
        alphabet = (dcs >> 2) & 0x03;
    } else if ((dcs & 0xC0) == 0x80) {
        alphabet = 2;
    } else {
        alphabet = 0;
    }

    if (alphabet == 2) {
        size_t chars = std::min<size_t>(udl / 2, ud.size() / 2);
        ud.resize(chars * 2);
        body = DecodeUcs2ToUtf8(ud);
        return true;
    }

    if (alphabet == 1) {
        size_t byte_count = std::min<size_t>(udl, ud.size());
        body.assign(ud.begin(), ud.begin() + static_cast<long>(byte_count));
        return true;
    }

    // Default GSM 7-bit.
    int bit_offset = 0;
    size_t septet_count = udl;
    if (has_udhi && !ud.empty()) {
        size_t udhl = ud[0];
        size_t header_total = udhl + 1;
        if (header_total > ud.size()) {
            return false;
        }
        bit_offset = static_cast<int>((header_total * 8) % 7);
        size_t consumed_header_septets = (header_total * 8 + 6) / 7;
        if (septet_count < consumed_header_septets) {
            return false;
        }
        septet_count -= consumed_header_septets;
        ud.erase(ud.begin(), ud.begin() + static_cast<long>(header_total));
    }

    size_t available_bits = ud.size() * 8;
    if (available_bits < static_cast<size_t>(bit_offset)) {
        return false;
    }
    size_t max_septets = (available_bits - static_cast<size_t>(bit_offset)) / 7;
    if (septet_count > max_septets) {
        septet_count = max_septets;
    }

    body = DecodeGsm7ToUtf8(ud, septet_count, bit_offset);
    return true;
}

bool AtController::ParseCmglHeader(const std::string &line, int &index_out) const
{
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    size_t cursor = colon_pos + 1;
    while (cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor]))) {
        ++cursor;
    }

    size_t start = cursor;
    while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor]))) {
        ++cursor;
    }
    if (cursor == start) {
        return false;
    }

    index_out = std::stoi(line.substr(start, cursor - start));
    return true;
}

bool AtController::IsHexLine(const std::string &line) const
{
    if (line.empty() || (line.size() % 2) != 0) {
        return false;
    }
    return std::all_of(line.begin(), line.end(), [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

bool AtController::HexToBytes(const std::string &hex, std::vector<uint8_t> &bytes) const
{
    if (!IsHexLine(hex)) {
        return false;
    }
    bytes.clear();
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned value = 0;
        if (std::sscanf(hex.c_str() + i, "%2x", &value) != 1) {
            bytes.clear();
            return false;
        }
        bytes.push_back(static_cast<uint8_t>(value));
    }
    return true;
}

std::string AtController::DecodeSemiOctetAddress(
    const std::vector<uint8_t> &bytes,
    size_t start,
    size_t digit_count,
    uint8_t toa) const
{
    std::string result;
    if ((toa & 0x70) == 0x10) {
        result.push_back('+');
    }

    size_t decoded_digits = 0;
    size_t octets = (digit_count + 1) / 2;
    for (size_t i = 0; i < octets; ++i) {
        uint8_t octet = bytes[start + i];
        uint8_t low = octet & 0x0F;
        uint8_t high = (octet >> 4) & 0x0F;

        if (decoded_digits < digit_count && low <= 9) {
            result.push_back(static_cast<char>('0' + low));
            ++decoded_digits;
        }
        if (decoded_digits < digit_count && high <= 9) {
            result.push_back(static_cast<char>('0' + high));
            ++decoded_digits;
        }
    }
    return result;
}

std::string AtController::DecodeUcs2ToUtf8(const std::vector<uint8_t> &ud) const
{
    std::string out;
    out.reserve(ud.size());
    for (size_t i = 0; i + 1 < ud.size(); i += 2) {
        uint16_t cp = static_cast<uint16_t>((ud[i] << 8) | ud[i + 1]);
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::string AtController::DecodeGsm7ToUtf8(
    const std::vector<uint8_t> &ud,
    size_t septet_count,
    int bit_offset) const
{
    std::string out;
    out.reserve(septet_count);
    for (size_t i = 0; i < septet_count; ++i) {
        size_t bit_index = static_cast<size_t>(bit_offset) + i * 7;
        size_t byte_index = bit_index / 8;
        int shift = static_cast<int>(bit_index % 8);
        if (byte_index >= ud.size()) {
            break;
        }

        uint16_t value = static_cast<uint16_t>(ud[byte_index] >> shift);
        if (shift > 1 && (byte_index + 1) < ud.size()) {
            value |= static_cast<uint16_t>(ud[byte_index + 1] << (8 - shift));
        }
        uint8_t septet = static_cast<uint8_t>(value & 0x7F);
        out += Gsm7ToUtf8Char(septet);
    }
    return out;
}

std::string AtController::Gsm7ToUtf8Char(uint8_t septet) const
{
    // Keep ASCII range exact and map CR/LF.
    if (septet >= 0x20 && septet <= 0x7E) {
        return std::string(1, static_cast<char>(septet));
    }
    if (septet == 0x0A) {
        return "\n";
    }
    if (septet == 0x0D) {
        return "\r";
    }
    if (septet == 0x00) {
        return "@";
    }
    return "?";
}

}  // namespace espclient