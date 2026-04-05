#pragma once

#include <functional>
#include <string>

namespace espclient {

enum class Method {
  GET,
  POST,
  PUT,
  DELETE_,
  PATCH,
};

class HttpsClient {
 public:
  HttpsClient();
  ~HttpsClient();

  void SetAuthToken(const std::string& token);
  void SendRequest(Method method,
                   const std::string& uri,
                   const std::string& payload,
                   std::function<void(int, std::string)> cb);

 private:
  std::string auth_token_;
};

}  // namespace espclient
