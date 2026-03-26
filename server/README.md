Server (Django)
----------------

Quick start (PowerShell):

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python manage.py migrate
python manage.py runserver 0.0.0.0:8000
```

Endpoints:
- `POST /potato/msg`  - JSON body with `TimeStamp`, `From`, `Data` (Data is base64 of IV||Ciphertext); requires `Authorization: Bearer <token>`
- `GET /potato/msg`   - Returns messages within last 24 hours
- `GET /potato/view.html` - Web page to input password and view decrypted messages

Configuration environment variables:
- `POTATO_AUTH_TOKEN` (required for `POST /potato/msg`)
- `POTATO_MYSQL_DATABASE` (default: `potato_db`)
- `POTATO_MYSQL_USER` (default: `root`)
- `POTATO_MYSQL_PASSWORD` (default: `root`)
- `POTATO_MYSQL_HOST` (default: `127.0.0.1`)
- `POTATO_MYSQL_PORT` (default: `3306`)
