# The web server.
The code for the web server is in the server folder. It is based on Django.
It has one web page and two APIs.

# APIs
- `POST /potato/msg`  
  Request Body:
  {
    "TimeStamp": "2025-10-11T22:29:03.123Z",
    "From": "13912345678",
    "Data": "4tqdKndH35nWIouW32lVITYudwU6fU2kDm8ZHAvFx28="
  } 
  Save the message into the table.
  Note: `Data` is a base64-encoded byte string of `IV || Ciphertext` (IV is the first 16 bytes).

- `GET /potato/msg`  
  Response Body:
  {
    "Messages":[
        {
            "Index": "12",
            "TimeStamp": "2025-10-11T22:29:03.234Z",
            "From": "13912345678",
            "Data": "4tqdKndH35nWIouW32lVITYudwU6fU2kDm8ZHAvFx28="

        },
        {
            "Index": "13",
            "TimeStamp": "2025-10-11T22:29:05.123Z",
            "From": "13912345678",
            "Data": "4tqdKndH35nWIouW32lVITYudwU6fU2kDm8ZHAvFx28="
        }
    ]
  }
  Fetches the messages which are within 24h. Data is already encrypted.
  Note: `Data` is returned as base64 of `IV || Ciphertext` (IV is the first 16 bytes).

# Database
It has only 1 table, potato
Schema:
| Column | Data Type |
|--------|-----------|
|Index| INT, AutoInc|
|TimeStamp| TIMESTAMP|
|From| TINYTEXT|
|Data|TEXT|

# Web Page
Url: /potato/view.html
It has a textbox to input the password. and a 'submit' button next to the textbox.
When the user click submit button:
* Calls GET /potato/msg to get the messages.
* Generate encryption key by calculate sha256 on <Password> + "potato". Password is UTF8 encoded.
* Decrypt Data using the key using AES256_CBC.
* Show the message on the page.