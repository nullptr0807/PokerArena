"""Difficulty control and noise injection for AI agents."""

from __future__ import annotations

from .agent import Difficulty


# Difficulty presets mapping
DIFFICULTY_PRESETS = {
    Difficulty.NORMAL: {
        "description": "普通玩家水平",
        "strategy": "Blueprint + 大量随机噪声",
        "epsilon": 0.30,  # exploration rate
        "leaks": ["limp_too_much", "fold_to_3bet_high", "overvalue_top_pair"],
    },
    Difficulty.MEDIUM: {
        "description": "非常强的人类玩家",
        "strategy": "Blueprint + 少量探索",
        "epsilon": 0.05,
        "leaks": [],
    },
    Difficulty.ADVANCED: {
        "description": "尽可能接近 GTO 最优",
        "strategy": "Blueprint + 实时子博弈求解",
        "epsilon": 0.0,
        "leaks": [],
    },
}


def get_difficulty_info(difficulty: Difficulty) -> dict:
    """Get description and parameters for a difficulty level."""
    return DIFFICULTY_PRESETS.get(difficulty, DIFFICULTY_PRESETS[Difficulty.NORMAL])
