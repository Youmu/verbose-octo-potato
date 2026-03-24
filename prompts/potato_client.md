# Class Potato Client
Class Name: PotatoClient

# Member Functions
- `PotatoClient(string potato_ep, string key)`  
  The potato_ep is the HTTP endpoint of the Potato Server.
  Key is the AES256 encryption key, base64 encoded.

- `PushMessage(string TimeStamp, string sender, string msg)`  
  Generate random IV. Encrypt the msg using AES256_CBC with the given key.
  Encode `IV || Ciphertext` using base64 encoding.
  Post to potato_ep using the following request body:
  ```json
  {
    "TimeStamp": "2025-10-11T22:29:03.123Z",
    "From": "13912345678",
    "Data": "4tqdKndH35nWIouW32lVITYudwU6fU2kDm8ZHAvFx28="
  } 
  ```

