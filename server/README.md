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
- `POST /potato/msg`  - JSON body with `TimeStamp`, `From`, `Data` (Data is base64 of IV||Ciphertext)
- `GET /potato/msg`   - Returns messages within last 24 hours
- `GET /potato/view.html` - Web page to input password and view decrypted messages

Database: sqlite `db.sqlite3` in this folder after migrations.
