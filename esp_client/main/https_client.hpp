#pragma once

#include <functional>
#include <string>
#include <queue>
#include <mutex>

namespace espclient {

enum class Method {
  GET,
  POST,
  PUT,
  DELETE_,
  PATCH,
};

class HttpsRequest {
 public:
  Method method;
  std::string uri;
  std::string payload;
  std::function<void(int, std::string)> callback;
};

class HttpsClient {
 public:
  HttpsClient();
  ~HttpsClient();

  void SetAuthToken(const std::string& token);
  void Start();
  void PushRequest(const HttpsRequest& request);

 private:
  std::queue<HttpsRequest> request_queue_;
  std::mutex queue_mutex_;

  static void HttpTaskFunction(void* param);
  void SendRequest(Method method,
                   const std::string& uri,
                   const std::string& payload,
                   std::function<void(int, std::string)> cb = nullptr);
  std::string auth_token_;
};

}  // namespace espclient
