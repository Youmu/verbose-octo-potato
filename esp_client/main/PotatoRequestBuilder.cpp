#include "PotatoRequestBuilder.hpp"
#include "mbedtls/base64.h"
#include "esp_log.h"
#include <ctime>
#include <sys/time.h>
#include <iomanip>
#include <sstream>

static const char *TAG = "REQ_BUILDER";

PotatoRequestBuilder::PotatoRequestBuilder(const std::string &key_base64) {
    uint8_t key_aes[32];
    size_t key_len = 32;
    int ret = mbedtls_base64_decode(key_aes, sizeof(key_aes), &key_len,
                                    reinterpret_cast<const unsigned char*>(key_base64.c_str()), key_base64.size());
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
        return;
    }

    auto psa_status = psa_crypto_init();
    if(psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed: %d", psa_status);
        return;
    }

    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&key_attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&key_attr, 256);
    psa_set_key_usage_flags(&key_attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&key_attr, PSA_ALG_CBC_PKCS7);

    psa_status = psa_import_key(&key_attr, key_aes, sizeof(key_aes), &key_id);

    if(psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Key import failed: %d", psa_status);
        return;
    }
}

std::string PotatoRequestBuilder::BuildRequest(const std::string &from, const std::string &message) {
    // Get current time with microseconds
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // Convert to tm structure
    struct tm *tm_info = gmtime(&tv.tv_sec);
    
    // Format as ISO 8601 with milliseconds: 2025-10-11T22:29:03.123Z
    std::stringstream ss;
    ss << std::setfill('0')
       << (tm_info->tm_year + 1900) << "-"
       << std::setw(2) << (tm_info->tm_mon + 1) << "-"
       << std::setw(2) << tm_info->tm_mday << "T"
       << std::setw(2) << tm_info->tm_hour << ":"
       << std::setw(2) << tm_info->tm_min << ":"
       << std::setw(2) << tm_info->tm_sec << "."
       << std::setw(3) << (tv.tv_usec / 1000) << "Z";
    std::string timeStamp = ss.str();


    uint8_t output[256]; // 16 bytes IV + 160 bytes ciphertext (for 128-byte message with PKCS7 padding)
    size_t output_len = 0;  

    auto psa_status = psa_cipher_encrypt(
        key_id,
        PSA_ALG_CBC_PKCS7,
        reinterpret_cast<const uint8_t*>(message.c_str()), message.size(),
        output, sizeof(output), &output_len
    );

    if(psa_status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Encryption failed: %d", psa_status);
        psa_destroy_key(key_id);
        return "";
    }

    unsigned char base64_output[240];
    size_t base64_output_len = 0;
    int ret = mbedtls_base64_encode(base64_output, sizeof(base64_output), &base64_output_len, output, output_len);
    if(ret != 0) {
        ESP_LOGE(TAG, "Base64 encoding failed: %d", ret);
        return "";
    }

    std::string msg = std::format(
        "{{\"TimeStamp\":\"{}\",\"From\":\"{}\",\"Data\":\"{}\"}}", 
        timeStamp,
        from,
        std::string(reinterpret_cast<char*>(base64_output), base64_output_len)
    );

    return msg;
}
