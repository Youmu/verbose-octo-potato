# The HTTPS Client
This is a https client wrapper for the ESP32 device.
Class Name: HttpsClient


# APIs

- `HttpsClient()`  
  Initialize the HTTPs module.

- `SetAuthToken(string token)`  
  Sets the bearer auth token.

- `SendRequest(Method method, string Uri, string payload, function<void(int, string)> cb)`  
  Sends an https request.

# Detailed Behavior
  If the auth token is set, the request must set the bearer auth token in request header.
  Bypass the server cert verification.