#include "mms_monitor/potato_client.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>
#include <string_view>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace mms_monitor {
namespace {

std::vector<unsigned char> Base64Decode(std::string_view input) {
  std::string compact;
  compact.reserve(input.size());
  for (const char ch : input) {
    if (std::isspace(static_cast<unsigned char>(ch)) == 0) {
      compact.push_back(ch);
    }
  }

  if (compact.empty() || (compact.size() % 4) != 0) {
    throw std::invalid_argument("Invalid base64 input length");
  }

  std::vector<unsigned char> decoded((compact.size() / 4) * 3);
  const int output_len = EVP_DecodeBlock(decoded.data(),
                                         reinterpret_cast<const unsigned char*>(compact.data()),
                                         static_cast<int>(compact.size()));
  if (output_len < 0) {
    throw std::invalid_argument("Invalid base64 input data");
  }

  int padding = 0;
  if (!compact.empty() && compact.back() == '=') {
    padding = 1;
    if (compact.size() >= 2 && compact[compact.size() - 2] == '=') {
      padding = 2;
    }
  }

  decoded.resize(static_cast<size_t>(output_len - padding));
  return decoded;
}

std::string Base64Encode(const std::vector<unsigned char>& bytes) {
  if (bytes.empty()) {
    return {};
  }
  std::string encoded(((bytes.size() + 2) / 3) * 4, '\0');
  const int len =
      EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()),
                      bytes.data(),
                      static_cast<int>(bytes.size()));
  if (len < 0) {
    throw std::runtime_error("Base64 encode failed");
  }
  encoded.resize(static_cast<size_t>(len));
  return encoded;
}

std::vector<unsigned char> EncryptAes256Cbc(const std::string& plaintext,
                                            const std::array<unsigned char, 32>& key,
                                            const std::array<unsigned char, 16>& iv) {
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("EVP_CIPHER_CTX_new failed");
  }

  std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
  int out_len = 0;
  int total_len = 0;

  const int init_ok =
      EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data());
  if (init_ok != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("EVP_EncryptInit_ex failed");
  }

  const int update_ok = EVP_EncryptUpdate(
      ctx,
      ciphertext.data(),
      &out_len,
      reinterpret_cast<const unsigned char*>(plaintext.data()),
      static_cast<int>(plaintext.size()));
  if (update_ok != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("EVP_EncryptUpdate failed");
  }
  total_len = out_len;

  const int final_ok = EVP_EncryptFinal_ex(ctx, ciphertext.data() + total_len, &out_len);
  EVP_CIPHER_CTX_free(ctx);
  if (final_ok != 1) {
    throw std::runtime_error("EVP_EncryptFinal_ex failed");
  }
  total_len += out_len;
  ciphertext.resize(static_cast<size_t>(total_len));
  return ciphertext;
}

void EnsureCurlGlobalInitialized() {
  static std::once_flag curl_once;
  static CURLcode curl_init_result = CURLE_OK;
  std::call_once(curl_once, []() { curl_init_result = curl_global_init(CURL_GLOBAL_DEFAULT); });
  if (curl_init_result != CURLE_OK) {
    throw std::runtime_error("curl_global_init failed");
  }
}

}  // namespace

PotatoClient::PotatoClient(std::string potato_ep, std::string key)
    : potato_ep_(std::move(potato_ep)), key_base64_(std::move(key)) {
  EnsureCurlGlobalInitialized();

  const std::vector<unsigned char> decoded_key = Base64Decode(key_base64_);
  if (decoded_key.size() != key_bytes_.size()) {
    throw std::invalid_argument("AES-256 key must decode to 32 bytes");
  }
  std::copy(decoded_key.begin(), decoded_key.end(), key_bytes_.begin());
}

void PotatoClient::PushMessage(std::string timestamp, std::string sender, std::string msg) {
  std::array<unsigned char, 16> iv{};
  if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
    throw std::runtime_error("RAND_bytes failed");
  }

  const std::vector<unsigned char> ciphertext = EncryptAes256Cbc(msg, key_bytes_, iv);
  std::vector<unsigned char> payload_bytes;
  payload_bytes.reserve(iv.size() + ciphertext.size());
  payload_bytes.insert(payload_bytes.end(), iv.begin(), iv.end());
  payload_bytes.insert(payload_bytes.end(), ciphertext.begin(), ciphertext.end());
  const std::string data = Base64Encode(payload_bytes);

  nlohmann::json body = {
      {"TimeStamp", timestamp},
      {"From", sender},
      {"Data", data},
  };
  const std::string body_text = body.dump();

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    throw std::runtime_error("curl_easy_init failed");
  }

  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, potato_ep_.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_text.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_text.size()));
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

  const CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    throw std::runtime_error(std::string("HTTP request failed: ") + curl_easy_strerror(res));
  }
  if (http_code < 200 || http_code >= 300) {
    throw std::runtime_error("HTTP request returned non-success status: " + std::to_string(http_code));
  }
}

}  // namespace mms_monitor
