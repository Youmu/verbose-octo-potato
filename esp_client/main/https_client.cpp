#include "https_client.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


namespace espclient {

constexpr const char* TAG = "HttpsClient";

HttpsClient::HttpsClient() {
  ESP_LOGI(TAG, "HttpsClient initialized");
}

HttpsClient::~HttpsClient() = default;

const char* HttpsClient::MethodToString(Method method) {
  switch (method) {
    case Method::GET:
      return "GET";
    case Method::POST:
      return "POST";
    case Method::PUT:
      return "PUT";
    case Method::DELETE_:
      return "DELETE";
    case Method::PATCH:
      return "PATCH";
    default:
      return "GET";
  }
}

esp_http_client_method_t MethodToHttpMethod(Method method) {
  switch (method) {
    case Method::GET:
      return HTTP_METHOD_GET;
    case Method::POST:
      return HTTP_METHOD_POST;
    case Method::PUT:
      return HTTP_METHOD_PUT;
    case Method::DELETE_:
      return HTTP_METHOD_DELETE;
    case Method::PATCH:
      return HTTP_METHOD_PATCH;
    default:
      return HTTP_METHOD_GET;
  }
}

void HttpsClient::SetAuthToken(const std::string& token) {
  auth_token_ = token;
}

void HttpsClient::SendRequest(Method method,
                              const std::string& uri,
                              const std::string& payload,
                              std::function<void(int, std::string)> cb) {
    char buf[2048];
    esp_http_client_config_t cfg = {};
    cfg.url = uri.c_str();
    cfg.skip_cert_common_name_check = true;  // Skip common name check for testing
    cfg.timeout_ms = 10000;  // Set timeout to 10 seconds

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_method(client, MethodToHttpMethod(method));
    esp_err_t err = esp_http_client_open(client, 0);
    int64_t content_length;
    if (err == ESP_OK) {
      content_length = esp_http_client_fetch_headers(client);
      if(content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers: %lld", content_length);
        }
        else {
          ESP_LOGI(TAG, "Content length: %lld", content_length);
          int data_read = esp_http_client_read_response(client, buf, 2048);
            if (data_read >= 0) {
              int status_code = esp_http_client_get_status_code(client);
              int cl = esp_http_client_get_content_length(client);
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld", status_code, cl);
                buf[content_length] = '\0';  // Null-terminate the buffer
                cb(status_code, std::string(buf));
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        cb(-1, "");
    } 
    esp_http_client_cleanup(client);
  } 

}  // namespace espclient
