"""API endpoints for player statistics."""

from __future__ import annotations

from fastapi import APIRouter

router = APIRouter(prefix="/api/stats", tags=["stats"])


# In-memory stats store (would be database in production)
_session_stats: dict[str, dict] = {}


def update_stats(session_id: str, players: list[dict]) -> None:
    """Update stats after a hand completes."""
    if session_id not in _session_stats:
        _session_stats[session_id] = {
            "hands_played": 0,
            "players": {},
        }
    stats = _session_stats[session_id]
    stats["hands_played"] += 1

    for p in players:
        idx = str(p["index"])
        if idx not in stats["players"]:
            stats["players"][idx] = {
                "name": p["name"],
                "hands_played": 0,
                "vpip_count": 0,
                "pfr_count": 0,
                "total_won": 0,
                "total_lost": 0,
                "biggest_pot_won": 0,
            }
        ps = stats["players"][idx]
        ps["hands_played"] += 1
        if p.get("vpip"):
            ps["vpip_count"] += 1
        if p.get("pfr"):
            ps["pfr_count"] += 1


def record_win(session_id: str, player_idx: int, amount: int) -> None:
    """Record a win for a player."""
    if session_id in _session_stats:
        idx = str(player_idx)
        ps = _session_stats[session_id]["players"].get(idx)
        if ps:
            ps["total_won"] += amount
            ps["biggest_pot_won"] = max(ps["biggest_pot_won"], amount)


@router.get("/{session_id}")
async def get_session_stats(session_id: str) -> dict:
    """Get statistics for a game session."""
    if session_id not in _session_stats:
        return {"error": "Session not found"}

    stats = _session_stats[session_id]
    result = {
        "hands_played": stats["hands_played"],
        "players": [],
    }

    for idx, ps in stats["players"].items():
        hands = max(ps["hands_played"], 1)
        result["players"].append({
            "index": int(idx),
            "name": ps["name"],
            "hands_played": ps["hands_played"],
            "vpip": round(ps["vpip_count"] / hands * 100, 1),
            "pfr": round(ps["pfr_count"] / hands * 100, 1),
            "total_won": ps["total_won"],
            "net_profit": ps["total_won"] - ps["total_lost"],
            "biggest_pot_won": ps["biggest_pot_won"],
        })

    return result


@router.get("/")
async def list_sessions() -> list[str]:
    """List all active session IDs with stats."""
    return list(_session_stats.keys())
