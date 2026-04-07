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

void HttpsClient::HttpTaskFunction(void* param) {
  // This function can be used to process queued HTTP requests if needed
  HttpsClient &self = *static_cast<HttpsClient*>(param);
  while (true) {
    // Process requests from a queue (not implemented in this example)
    self.queue_mutex_.lock();
    if(self.request_queue_.empty()) {
      self.queue_mutex_.unlock();
      vTaskDelay(pdMS_TO_TICKS(1000));  // Sleep for a while if no pending requests
      continue;
    }
    auto request = self.request_queue_.front();
    self.request_queue_.pop();
    self.queue_mutex_.unlock();
    self.SendRequest(request.method, request.uri, request.payload);
  } 
}

void HttpsClient::Start() {
  ESP_LOGI(TAG, "HttpsClient started");
  BaseType_t created = xTaskCreate(
                HttpTaskFunction,
                "HttpEventLoop",
                8192,
                nullptr,
                5,
                nullptr);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HttpEventLoop task");
    }
}

void HttpsClient::PushRequest(const HttpsRequest& request) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  request_queue_.push(request);
}

void HttpsClient::SendRequest(Method method,
                              const std::string& uri,
                              const std::string& payload,
                              std::function<void(int, std::string)> cb) {
    char buf[2048];
    esp_err_t err;
    int64_t content_length;
    bool withPayload = false;
    int data_read;
    esp_http_client_config_t cfg = {};
    cfg.url = uri.c_str();
    cfg.skip_cert_common_name_check = true;  // Skip common name check for testing
    cfg.timeout_ms = 10000;  // Set timeout to 10 seconds

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_method(client, MethodToHttpMethod(method));
    if(!auth_token_.empty()) {
      std::string header_value = "Bearer " + auth_token_;
      esp_http_client_set_header(client, "Authorization", header_value.c_str());
    }

    if(method == Method::POST || method == Method::PUT || method == Method::PATCH) {
      err = esp_http_client_open(client, payload.size());
      withPayload = true;
    }
    else {
      err = esp_http_client_open(client, 0);
      withPayload = false;
    }
    if(err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
      goto error;
    }
    
    if(withPayload) {
      int wlen = esp_http_client_write(client, payload.c_str(), payload.size());
      if(wlen < 0) {
        ESP_LOGE(TAG, "Failed to write payload: %d", wlen);
        goto error;
      }
    }

    content_length = esp_http_client_fetch_headers(client);
    if(content_length < 0) {
      ESP_LOGE(TAG, "Failed to fetch headers: %lld", content_length);
      goto error;
    }
    data_read = esp_http_client_read_response(client, buf, 2048);
    if(data_read < 0) {
      ESP_LOGE(TAG, "Failed to read response: %d", data_read);
      goto error;
    }
    {
      int status_code = esp_http_client_get_status_code(client);
      int cl = esp_http_client_get_content_length(client);
      ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld", status_code, cl);
      buf[content_length] = '\0';  // Null-terminate the buffer
      cb(status_code, std::string(buf));
      esp_http_client_cleanup(client);
      return;
    }
  error:
    cb(-1, "");
    esp_http_client_cleanup(client);
  } 

}  // namespace espclient
