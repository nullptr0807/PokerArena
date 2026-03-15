"""AI decision-making agent for poker gameplay.

Provides a Python-based AI agent that makes decisions based on difficulty level.
This serves as the primary AI until the C++ GTO engine is integrated.
"""

from __future__ import annotations

import secrets
from enum import Enum, auto
from dataclasses import dataclass


class Difficulty(Enum):
    NORMAL = "normal"      # 普通: Blueprint + 大量噪声, 模拟常见 leak
    MEDIUM = "medium"      # 中等: Blueprint + 少量探索, 接近 GTO
    ADVANCED = "advanced"  # 高级: Blueprint + 实时子博弈求解


@dataclass
class GameContext:
    """Snapshot of relevant game state for AI decision-making."""
    hole_cards: list[int]
    board: list[int]
    pot: int
    current_bet: int
    my_bet_this_street: int
    my_chips: int
    num_players_in_hand: int
    street: str  # PREFLOP, FLOP, TURN, RIVER
    position: str  # early, middle, late, blinds


def _random_float() -> float:
    """Cryptographically secure random float in [0, 1)."""
    return secrets.randbelow(10000) / 10000.0


class AIAgent:
    """
    AI poker agent with configurable difficulty.
    
    Current implementation uses heuristic-based strategy.
    Future: integrate C++ MCCFR engine via pybind11 for GTO play.
    """

    def __init__(self, difficulty: Difficulty = Difficulty.NORMAL) -> None:
        self.difficulty = difficulty

    def decide(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        """
        Decide an action given game context and valid actions.
        
        Returns:
            {"action": "fold"|"check"|"call"|"raise"|"all_in", "amount": int}
        """
        # Route multiplayer pots to specialized strategy (medium/advanced only)
        if ctx.num_players_in_hand > 2 and self.difficulty in (Difficulty.MEDIUM, Difficulty.ADVANCED):
            return self._decide_multiplayer(ctx, valid_actions)

        if self.difficulty == Difficulty.NORMAL:
            return self._decide_normal(ctx, valid_actions)
        elif self.difficulty == Difficulty.MEDIUM:
            return self._decide_medium(ctx, valid_actions)
        else:
            return self._decide_advanced(ctx, valid_actions)

    # ─── Multiplayer strategy ────────────────────────────────────

    def _decide_multiplayer(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        """Strategy for 3+ player pots using blueprint adaptation."""
        from . import engine_bridge

        num_opps = ctx.num_players_in_hand - 1

        # Get hand strength if engine available
        if engine_bridge.is_available():
            ehs = engine_bridge.compute_ehs(ctx.hole_cards[:2], ctx.board, 1000)
        else:
            ehs = 0.5  # unknown

        to_call = ctx.current_bet - ctx.my_bet_this_street
        pot_odds = to_call / max(ctx.pot + to_call, 1) if to_call > 0 else 0

        # Tighter ranges with more opponents
        ehs_threshold_call = 0.4 + 0.05 * num_opps
        ehs_threshold_raise = 0.6 + 0.05 * num_opps

        r = _random_float()

        if to_call <= 0:
            # No bet to face
            if "check" in valid_actions:
                if ehs > ehs_threshold_raise and "raise" in valid_actions:
                    if r < 0.7:
                        return self._make_raise(ctx, size_fraction=0.66)
                # Mix in some bets with medium hands for balance
                if ehs > 0.55 and "raise" in valid_actions and r < 0.25:
                    return self._make_raise(ctx, size_fraction=0.5)
                return {"action": "check", "amount": 0}

        # Facing a bet
        if ehs > ehs_threshold_raise and "raise" in valid_actions:
            # Strong hand: raise
            if r < 0.6:
                return self._make_raise(ctx, size_fraction=0.75)
            return {"action": "call", "amount": 0} if "call" in valid_actions else {"action": "fold", "amount": 0}

        if ehs > ehs_threshold_call:
            # Decent hand: call if pot odds are right
            if pot_odds < ehs * 0.8:
                if "call" in valid_actions:
                    return {"action": "call", "amount": 0}
            # Marginal: sometimes call, sometimes fold
            if r < 0.3 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}

        # Weak hand
        bluff_freq = 0.1 / num_opps
        if r < bluff_freq and "raise" in valid_actions:
            return self._make_raise(ctx, size_fraction=0.66)

        return {"action": "fold", "amount": 0}

    # ─── Normal difficulty ────────────────────────────────────────
    # 模拟普通玩家: limp 过多, fold to 3bet 过高, 随机性大

    def _decide_normal(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        r = _random_float()
        to_call = ctx.current_bet - ctx.my_bet_this_street

        # No bet facing us
        if to_call <= 0:
            if "check" in valid_actions:
                if r < 0.6:
                    return {"action": "check", "amount": 0}
                elif r < 0.85 and "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=0.5)
                elif "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=0.75)
                return {"action": "check", "amount": 0}

        # Facing a bet
        pot_odds = to_call / max(ctx.pot + to_call, 1)

        if r < 0.35:
            # Loose call tendency (leak: calling too wide)
            if "call" in valid_actions:
                return {"action": "call", "amount": 0}
        if r < 0.55:
            # Fold to aggression (leak: folding too much)
            return {"action": "fold", "amount": 0}
        if r < 0.80:
            if "call" in valid_actions:
                return {"action": "call", "amount": 0}
        if "raise" in valid_actions and r < 0.92:
            return self._make_raise(ctx, size_fraction=0.6)

        if "call" in valid_actions:
            return {"action": "call", "amount": 0}
        return {"action": "fold", "amount": 0}

    # ─── Medium difficulty ────────────────────────────────────────
    # 强人类玩家: 接近 GTO, 少量探索

    def _decide_medium(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        r = _random_float()
        to_call = ctx.current_bet - ctx.my_bet_this_street
        pot_odds = to_call / max(ctx.pot + to_call, 1)

        if to_call <= 0:
            if "check" in valid_actions:
                # C-bet / donk bet logic
                if r < 0.35 and "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=0.66)
                return {"action": "check", "amount": 0}

        # Facing a bet: use pot odds heuristic with small epsilon exploration
        epsilon = 0.05
        if r < epsilon:
            # Random exploration
            actions = [a for a in valid_actions if a != "fold"]
            if actions:
                chosen = actions[secrets.randbelow(len(actions))]
                if chosen == "raise":
                    return self._make_raise(ctx, size_fraction=0.75)
                return {"action": chosen, "amount": 0}

        if pot_odds < 0.25:
            # Good odds, lean toward calling/raising
            if r < 0.6 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            if "raise" in valid_actions:
                return self._make_raise(ctx, size_fraction=0.75)
            if "call" in valid_actions:
                return {"action": "call", "amount": 0}
        elif pot_odds < 0.40:
            if r < 0.5 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}
        else:
            # Bad odds
            if r < 0.15 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}

        return {"action": "fold", "amount": 0}

    # ─── Advanced difficulty ──────────────────────────────────────
    # GTO 近似: 使用 C++ MCCFR 子博弈求解器

    def _decide_advanced(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        from . import engine_bridge

        if engine_bridge.is_available() and engine_bridge._solver is not None:
            # Use C++ subgame solver (only when blueprint is loaded)
            probs = engine_bridge.solve_subgame(
                hole_cards=ctx.hole_cards[:2],
                board=ctx.board,
                pot=ctx.pot,
                current_bet=ctx.current_bet,
                my_chips=ctx.my_chips,
                opp_chips=max(ctx.pot, 200),  # estimate
                iterations=200,
            )
            return engine_bridge.pick_action(probs, valid_actions, ctx)
        else:
            # No blueprint — use enhanced medium strategy with tighter ranges
            return self._decide_medium(ctx, valid_actions)

    # ─── Helpers ──────────────────────────────────────────────────

    def _make_raise(self, ctx: GameContext, size_fraction: float = 0.66) -> dict:
        """Create a raise action sized as a fraction of the pot."""
        pot = ctx.pot
        raise_amount = max(
            ctx.current_bet * 2,  # minimum: 2x current bet
            int(pot * size_fraction) + ctx.current_bet,
        )
        # Cap at all-in
        max_raise = ctx.my_chips + ctx.my_bet_this_street
        raise_amount = min(raise_amount, max_raise)

        if raise_amount >= ctx.my_chips + ctx.my_bet_this_street:
            return {"action": "all_in", "amount": 0}

        return {"action": "raise", "amount": raise_amount}
