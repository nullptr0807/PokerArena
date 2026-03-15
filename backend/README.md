# Poker Arena Backend

## Setup
```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## Run Tests
```bash
python -m pytest tests/ -v
```

## Run Server
```bash
uvicorn api.server:app --reload
```
