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
  Authentication is requrired. Using bearer authentication. The token is loaded from the env variable POTATO_AUTH_TOKEN
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
* Save the generated key in cookie, no expire time. If the key can be found in cookie, the password textbox shows some place-holder.
* Decrypt Data using the key using AES256_CBC.
* Show the message on the page.

About the UI:
* Use bootstrap theme to make the page works on both PC and phone.
* List the message in order that most recent on top.
* Each item has a header and content. The header is time and sender. Show the time in format: MM-DD HH:MM:SS in client timezone.
* The content is the decrypted message.

# Configuration Environment Variables
| Variable | Description | Default |
|----------|-------------|---------|
| POTATO_AUTH_TOKEN | Bearer token required by `POST /potato/msg` | (required, no default) |
| POTATO_MYSQL_DATABASE | MySQL database name | `potato_db` |
| POTATO_MYSQL_USER | MySQL user | `root` |
| POTATO_MYSQL_PASSWORD | MySQL password | `root` |
| POTATO_MYSQL_HOST | MySQL host | `127.0.0.1` |
| POTATO_MYSQL_PORT | MySQL port | `3306` |