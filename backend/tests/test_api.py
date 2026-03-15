"""Tests for the FastAPI server and WebSocket handler."""

import pytest
from api.server import app


def test_health_endpoint():
    from fastapi.testclient import TestClient
    client = TestClient(app)
    resp = client.get("/health")
    assert resp.status_code == 200
    assert resp.json() == {"status": "ok"}


def test_difficulties_endpoint():
    from fastapi.testclient import TestClient
    client = TestClient(app)
    resp = client.get("/api/difficulties")
    assert resp.status_code == 200
    data = resp.json()
    assert len(data) == 3
    ids = [d["id"] for d in data]
    assert "normal" in ids
    assert "medium" in ids
    assert "advanced" in ids


def test_game_info_endpoint():
    from fastapi.testclient import TestClient
    client = TestClient(app)
    resp = client.get("/api/game-info")
    assert resp.status_code == 200
    data = resp.json()
    assert data["player_range"] == [2, 6]
    assert data["run_it_times_max"] == 3


def test_websocket_ping():
    from fastapi.testclient import TestClient
    client = TestClient(app)
    with client.websocket_connect("/ws/game") as ws:
        ws.send_json({"type": "ping"})
        resp = ws.receive_json()
        assert resp["type"] == "pong"


def test_websocket_create_game():
    from fastapi.testclient import TestClient
    client = TestClient(app)
    with client.websocket_connect("/ws/game") as ws:
        ws.send_json({
            "type": "create_game",
            "num_players": 2,
            "small_blind": 1,
            "big_blind": 2,
            "starting_chips": 400,
            "ai_difficulties": ["normal"],
        })
        resp = ws.receive_json()
        assert resp["type"] == "game_created"
        assert resp["data"]["num_players"] == 2
