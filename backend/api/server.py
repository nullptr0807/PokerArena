"""FastAPI main server entry point for PokerArena."""

from __future__ import annotations

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .ws import router as ws_router
from .stats import router as stats_router

app = FastAPI(
    title="PokerArena",
    description="Texas Hold'em No-Limit Cash Game — Human vs AI",
    version="0.1.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # tighten for production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(ws_router)
app.include_router(stats_router)


@app.get("/health")
async def health() -> dict:
    return {"status": "ok"}


@app.get("/api/difficulties")
async def difficulties() -> list[dict]:
    """Return available AI difficulty levels."""
    return [
        {"id": "normal", "name": "普通", "description": "普通玩家水平 — Blueprint + 随机噪声"},
        {"id": "medium", "name": "中等", "description": "非常强的人类玩家 — 接近 GTO"},
        {"id": "advanced", "name": "高级", "description": "尽可能接近 GTO 最优 — 实时子博弈求解"},
    ]


@app.get("/api/game-info")
async def game_info() -> dict:
    """Return game configuration options."""
    return {
        "player_range": [2, 6],
        "blind_options": [{"small": 1, "big": 2}, {"small": 2, "big": 5}, {"small": 5, "big": 10}],
        "starting_chips_default": 400,
        "run_it_times_max": 3,
    }


def main() -> None:
    uvicorn.run("api.server:app", host="0.0.0.0", port=8000, reload=True)


if __name__ == "__main__":
    main()
