#include "mms_monitor/at_controller.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mms_monitor {
namespace {

std::string_view TrimAscii(std::string_view input) {
  while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())) != 0) {
    input.remove_prefix(1);
  }
  while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back())) != 0) {
    input.remove_suffix(1);
  }
  return input;
}

bool StartsWithCaseInsensitive(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  return std::equal(prefix.begin(), prefix.end(), text.begin(), [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  });
}

bool EqualsCaseInsensitive(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  return std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
  });
}

int HexNibble(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

bool HexToBytes(std::string_view hex, std::vector<uint8_t>* out) {
  out->clear();
  if ((hex.size() % 2) != 0) {
    return false;
  }
  out->reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    const int hi = HexNibble(hex[i]);
    const int lo = HexNibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    out->push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return true;
}

char Gsm7ToChar(uint8_t value) {
  switch (value) {
    case 0x00:
      return '@';
    case 0x0A:
      return '\n';
    case 0x0D:
      return '\r';
    default:
      if (value >= 0x20 && value <= 0x7E) {
        return static_cast<char>(value);
      }
      return '?';
  }
}

void AppendUtf8(uint32_t codepoint, std::string* out) {
  if (codepoint <= 0x7F) {
    out->push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out->push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out->push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0x10FFFF) {
    out->push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out->push_back('?');
  }
}

std::string DecodeUcs2ToUtf8(const std::vector<uint8_t>& bytes, size_t octets) {
  std::string out;
  if (octets > bytes.size()) {
    octets = bytes.size();
  }
  out.reserve(octets);

  for (size_t i = 0; i + 1 < octets; i += 2) {
    const uint16_t value =
        static_cast<uint16_t>((static_cast<uint16_t>(bytes[i]) << 8) | bytes[i + 1]);
    // UCS-2 is BMP-only; map invalid surrogate code points to replacement.
    if (value >= 0xD800 && value <= 0xDFFF) {
      AppendUtf8(0xFFFD, &out);
      continue;
    }
    AppendUtf8(value, &out);
  }
  return out;
}

std::string DecodeGsm7(const std::vector<uint8_t>& bytes, size_t septet_count, size_t skip_bits) {
  std::string out;
  out.reserve(septet_count);
  bool escape = false;

  for (size_t i = 0; i < septet_count; ++i) {
    const size_t bit_index = skip_bits + (i * 7);
    const size_t byte_index = bit_index / 8;
    const size_t bit_offset = bit_index % 8;
    if (byte_index >= bytes.size()) {
      break;
    }

    uint16_t value = static_cast<uint16_t>(bytes[byte_index] >> bit_offset);
    if (bit_offset > 1 && (byte_index + 1) < bytes.size()) {
      value |= static_cast<uint16_t>(bytes[byte_index + 1] << (8 - bit_offset));
    }
    const uint8_t septet = static_cast<uint8_t>(value & 0x7F);

    if (escape) {
      // Keep extension handling minimal; unsupported extended chars become '?'.
      out.push_back('?');
      escape = false;
      continue;
    }
    if (septet == 0x1B) {
      escape = true;
      continue;
    }
    out.push_back(Gsm7ToChar(septet));
  }

  return out;
}

std::string DecodeSemiOctetAddress(const uint8_t* bytes, size_t byte_len, int digit_count, uint8_t toa) {
  std::string out;
  out.reserve(static_cast<size_t>(digit_count) + 1);

  const bool international = (toa & 0xF0) == 0x90;
  if (international) {
    out.push_back('+');
  }

  const auto append_nibble = [&out](uint8_t nibble) {
    if (nibble <= 9) {
      out.push_back(static_cast<char>('0' + nibble));
    } else if (nibble == 0x0A) {
      out.push_back('*');
    } else if (nibble == 0x0B) {
      out.push_back('#');
    } else {
      out.push_back('?');
    }
  };

  int produced = 0;
  for (size_t i = 0; i < byte_len && produced < digit_count; ++i) {
    const uint8_t b = bytes[i];
    const uint8_t low = static_cast<uint8_t>(b & 0x0F);
    const uint8_t high = static_cast<uint8_t>((b >> 4) & 0x0F);

    if (low != 0x0F && produced < digit_count) {
      append_nibble(low);
      ++produced;
    }
    if (high != 0x0F && produced < digit_count) {
      append_nibble(high);
      ++produced;
    }
  }
  return out;
}

struct ParsedPdu {
  std::string sender;
  std::string body;
  bool ok{false};
};

ParsedPdu ParseDeliverPdu(std::string_view hex_pdu) {
  ParsedPdu parsed{};
  std::vector<uint8_t> bytes;
  if (!HexToBytes(hex_pdu, &bytes) || bytes.empty()) {
    return parsed;
  }

  size_t pos = 0;
  const size_t sca_len = bytes[pos++];
  if (pos + sca_len > bytes.size()) {
    return parsed;
  }
  pos += sca_len;
  if (pos >= bytes.size()) {
    return parsed;
  }

  const uint8_t first_octet = bytes[pos++];
  const uint8_t mti = static_cast<uint8_t>(first_octet & 0x03);
  if (mti != 0x00) {
    // Expect SMS-DELIVER from CMGL inbox.
    return parsed;
  }
  const bool udhi = (first_octet & 0x40) != 0;

  if ((pos + 2) > bytes.size()) {
    return parsed;
  }
  const int oa_len = bytes[pos++];
  const uint8_t oa_toa = bytes[pos++];

  size_t oa_byte_len = 0;
  std::string sender;
  if ((oa_toa & 0x70) == 0x50) {
    // Alphanumeric address (GSM 7-bit), oa_len is in septets.
    oa_byte_len = (static_cast<size_t>(oa_len) * 7 + 7) / 8;
    if (pos + oa_byte_len > bytes.size()) {
      return parsed;
    }
    std::vector<uint8_t> oa_bytes(bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                                  bytes.begin() + static_cast<std::ptrdiff_t>(pos + oa_byte_len));
    sender = DecodeGsm7(oa_bytes, static_cast<size_t>(oa_len), 0);
  } else {
    // Numeric semi-octet address.
    oa_byte_len = static_cast<size_t>((oa_len + 1) / 2);
    if (pos + oa_byte_len > bytes.size()) {
      return parsed;
    }
    sender = DecodeSemiOctetAddress(bytes.data() + pos, oa_byte_len, oa_len, oa_toa);
  }
  pos += oa_byte_len;

  if (pos + 9 > bytes.size()) {
    return parsed;
  }
  pos += 1;  // PID
  const uint8_t dcs = bytes[pos++];
  pos += 7;  // SCTS

  const int udl = bytes[pos++];
  if (udl < 0 || pos > bytes.size()) {
    return parsed;
  }
  const std::vector<uint8_t> ud_bytes(bytes.begin() + static_cast<std::ptrdiff_t>(pos), bytes.end());

  std::string body;
  const uint8_t dcs_group = static_cast<uint8_t>(dcs & 0x0C);
  if (dcs_group == 0x08) {
    // UCS2 payload converted to UTF-8.
    body = DecodeUcs2ToUtf8(ud_bytes, static_cast<size_t>(udl));
  } else if (dcs_group == 0x04) {
    // 8-bit data: expose as printable bytes where possible.
    size_t octets = static_cast<size_t>(udl);
    if (octets > ud_bytes.size()) {
      octets = ud_bytes.size();
    }
    body.reserve(octets);
    for (size_t i = 0; i < octets; ++i) {
      const uint8_t b = ud_bytes[i];
      body.push_back((b >= 0x20 && b <= 0x7E) ? static_cast<char>(b) : '?');
    }
  } else {
    // Default alphabet (GSM 7-bit).
    size_t skip_bits = 0;
    size_t septets = static_cast<size_t>(udl);
    if (udhi && !ud_bytes.empty()) {
      const size_t udh_octets = static_cast<size_t>(ud_bytes[0]) + 1;
      skip_bits = udh_octets * 8;
      const size_t skip_septets = (skip_bits + 6) / 7;
      if (septets > skip_septets) {
        septets -= skip_septets;
      } else {
        septets = 0;
      }
    }
    body = DecodeGsm7(ud_bytes, septets, skip_bits);
  }

  parsed.sender = sender.empty() ? "UNKNOWN" : std::move(sender);
  parsed.body = std::move(body);
  parsed.ok = true;
  return parsed;
}

int ParseUnsignedInt(std::string_view token) {
  token = TrimAscii(token);
  if (token.empty()) {
    return -1;
  }
  int value = 0;
  for (const char ch : token) {
    if (ch < '0' || ch > '9') {
      return -1;
    }
    value = value * 10 + (ch - '0');
  }
  return value;
}

std::pair<int, int> ParseCmglHeader(std::string_view line) {
  const size_t colon_pos = line.find(':');
  if (colon_pos == std::string_view::npos) {
    return {-1, -1};
  }

  std::string_view rest = TrimAscii(line.substr(colon_pos + 1));
  const size_t first_comma = rest.find(',');
  if (first_comma == std::string_view::npos) {
    return {ParseUnsignedInt(rest), -1};
  }

  const int index = ParseUnsignedInt(rest.substr(0, first_comma));

  rest = rest.substr(first_comma + 1);
  const size_t second_comma = rest.find(',');
  std::string_view status_token = second_comma == std::string_view::npos ? rest : rest.substr(0, second_comma);
  status_token = TrimAscii(status_token);
  if (!status_token.empty() && status_token.front() == '"' && status_token.back() == '"' &&
      status_token.size() >= 2) {
    status_token = status_token.substr(1, status_token.size() - 2);
  }
  const int message_status = ParseUnsignedInt(status_token);
  return {index, message_status};
}

}  // namespace

void AtController::Init() {
  if (send_cb_) {
    send_cb_("AT+CMGF=0");
  }
}

void AtController::ListSms(SmsStatus status) {
  if (!send_cb_) {
    return;
  }
  send_cb_("AT+CMGL=" + std::to_string(static_cast<int>(status)));
}

void AtController::SetOnSend(SendMessageCb sendCb) { send_cb_ = sendCb; }

void AtController::SetOnSmsReceived(SmsCb smsCb) { sms_cb_ = smsCb; }

void AtController::SetOnSmsNewMsg(NewMsgCb newMsgCb) { new_msg_cb_ = newMsgCb; }

void AtController::ReceiveMessage(std::string message) {
  ParseMessage(message);
}

void AtController::ParseMessage(std::string_view message) {
  const std::string_view line = TrimAscii(message);
  if (line.empty()) {
    return;
  }

  // Ignore command echo lines such as "AT+..."
  if (StartsWithCaseInsensitive(line, "at+")) {
    return;
  }

  // End of list response.
  if (EqualsCaseInsensitive(line, "OK")) {
    state_ = State::kInit;
    last_cmgl_index_ = -1;
    last_cmgl_message_status_ = -1;
    last_cmgl_body_.clear();
    return;
  }

  // Unsolicited or response header line from modem.
  if (line.front() == '+') {
    if (StartsWithCaseInsensitive(line, "+CMTI")) {
      if (new_msg_cb_) {
        new_msg_cb_();
      }
      return;
    }
    if (StartsWithCaseInsensitive(line, "+CMGL:")) {
      const auto [index, message_status] = ParseCmglHeader(line);
      state_ = State::kCmgl;
      last_cmgl_index_ = index;
      last_cmgl_message_status_ = message_status;
    }
    return;
  }

  // In CMGL state, next non-header line is the message body (hex string).
  if (state_ == State::kCmgl) {
    last_cmgl_body_.assign(line.begin(), line.end());
    if (sms_cb_) {
      const ParsedPdu parsed = ParseDeliverPdu(line);
      const std::string sender = parsed.ok ? parsed.sender : std::string("UNKNOWN");
      const std::string body = parsed.ok ? parsed.body : last_cmgl_body_;
      const bool handled = sms_cb_(sender, body);
      if (handled && send_cb_ && last_cmgl_index_ >= 0) {
        send_cb_("AT+CMGD=" + std::to_string(last_cmgl_index_));
      }
    }
  }
}

}  // namespace mms_monitor
