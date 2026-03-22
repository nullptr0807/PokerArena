"""AI decision-making agent for poker gameplay.

Provides a Python-based AI agent that makes decisions based on difficulty level.
This serves as the primary AI until the C++ GTO engine is integrated.
"""

from __future__ import annotations

import secrets
from enum import Enum, auto
from dataclasses import dataclass, field


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
    action_history: list[str] = field(default_factory=list)  # normalized action sequence (button=seat0)


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
        result = self._decide_inner(ctx, valid_actions)
        import logging
        logging.getLogger("ai.decide").warning(
            "%s | %s | cards=%s board=%s pot=%d bet=%d mybet=%d chips=%d | -> %s amt=%s",
            self.difficulty.value, ctx.street, ctx.hole_cards[:2], ctx.board,
            ctx.pot, ctx.current_bet, ctx.my_bet_this_street, ctx.my_chips,
            result["action"], result.get("amount", 0),
        )
        return result

    def _decide_inner(self, ctx: GameContext, valid_actions: list[str]) -> dict:
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
    # 模拟 NL50 reg: 有基本手牌选择, VPIP ~25-30%, PFR ~18-22%
    # Leaks: 翻后 c-bet 后容易放弃, 不够激进, 偶尔 overplay 中等牌

    def _decide_normal(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        from . import engine_bridge

        r = _random_float()
        to_call = ctx.current_bet - ctx.my_bet_this_street

        # Get hand strength
        if engine_bridge.is_available():
            ehs = engine_bridge.compute_ehs(ctx.hole_cards[:2], ctx.board, 500)
        else:
            ehs = 0.5

        # ── Preflop ──
        if ctx.street == "PREFLOP":
            return self._normal_preflop(ctx, valid_actions, ehs, r)

        # ── Postflop ──
        pot_odds = to_call / max(ctx.pot + to_call, 1) if to_call > 0 else 0

        if to_call <= 0:
            # No bet to face — check or bet
            if "check" in valid_actions:
                # Bet with strong hands, occasionally with medium
                if ehs > 0.7 and "raise" in valid_actions:
                    if r < 0.65:
                        return self._make_raise(ctx, size_fraction=0.6)
                if ehs > 0.5 and "raise" in valid_actions:
                    if r < 0.25:
                        return self._make_raise(ctx, size_fraction=0.5)
                return {"action": "check", "amount": 0}

        # Facing a bet
        if ehs > 0.75:
            # Strong hand: raise sometimes, mostly call
            if "raise" in valid_actions and r < 0.35:
                return self._make_raise(ctx, size_fraction=0.7)
            if "call" in valid_actions:
                return {"action": "call", "amount": 0}
        elif ehs > 0.55:
            # Decent hand: call if odds ok, fold to big bets (leak: not raising enough)
            if pot_odds < 0.35 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            if r < 0.3 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}
        elif ehs > 0.4:
            # Marginal: call small bets sometimes (leak: calling station tendencies)
            if pot_odds < 0.2 and r < 0.4 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}
        else:
            # Weak hand: fold, rare bluff
            if r < 0.06 and "raise" in valid_actions:
                return self._make_raise(ctx, size_fraction=0.66)
            return {"action": "fold", "amount": 0}

    def _normal_preflop(self, ctx: GameContext, valid_actions: list[str], ehs: float, r: float) -> dict:
        """Normal difficulty preflop: ~28% VPIP, ~20% PFR, fold to 3bet ~60%."""
        to_call = ctx.current_bet - ctx.my_bet_this_street
        is_facing_raise = to_call > ctx.pot * 0.15  # someone raised beyond blinds
        is_facing_3bet = to_call > ctx.pot * 0.4

        if to_call <= 0:
            # Unopened or checked to us
            if "check" in valid_actions:
                # Open raise with top ~22% of hands
                if ehs > 0.62 and "raise" in valid_actions:
                    if r < 0.80:
                        return self._make_raise(ctx, size_fraction=1.0)
                # Limp with marginal hands (leak)
                if ehs > 0.50 and r < 0.25:
                    return {"action": "check", "amount": 0}  # limp
                return {"action": "check", "amount": 0}

        # Facing a raise
        if is_facing_3bet:
            # Facing 3bet: tight, fold a lot (leak)
            if ehs > 0.72:
                if r < 0.4 and "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=1.0)  # 4bet
                if "call" in valid_actions:
                    return {"action": "call", "amount": 0}
            if ehs > 0.62 and r < 0.35 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}

        if is_facing_raise:
            # Facing open raise: call with ~top 28%, 3bet with ~top 12%
            if ehs > 0.70:
                if r < 0.40 and "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=1.0)  # 3bet
                if "call" in valid_actions:
                    return {"action": "call", "amount": 0}
            if ehs > 0.55 and "call" in valid_actions:
                if r < 0.70:
                    return {"action": "call", "amount": 0}
            return {"action": "fold", "amount": 0}

        # Facing a limp
        if ehs > 0.58 and "raise" in valid_actions:
            if r < 0.65:
                return self._make_raise(ctx, size_fraction=1.0)
            if "call" in valid_actions:
                return {"action": "call", "amount": 0}
        if ehs > 0.48 and "call" in valid_actions:
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
        import logging
        _log = logging.getLogger("ai.advanced")

        if not engine_bridge.is_available() or engine_bridge._solver is None:
            return self._decide_medium(ctx, valid_actions)

        # Preflop: use EHS-based strategy (subgame solving doesn't converge
        # with enough iterations for preflop's enormous game tree)
        if ctx.street == "PREFLOP":
            return self._advanced_preflop(ctx, valid_actions)

        # Postflop: real-time subgame solving with EHS sanity checks
        iters_by_street = {
            "FLOP": 5000,
            "TURN": 10000,
            "RIVER": 15000,
        }
        iterations = iters_by_street.get(ctx.street, 5000)

        ehs = engine_bridge.compute_ehs(ctx.hole_cards[:2], ctx.board, 1000)

        probs = engine_bridge.solve_subgame(
            hole_cards=ctx.hole_cards[:2],
            board=ctx.board,
            pot=ctx.pot,
            current_bet=ctx.current_bet,
            my_chips=ctx.my_chips,
            opp_chips=max(ctx.pot, 200),  # estimate
            iterations=iterations,
        )
        result = engine_bridge.pick_action(probs, valid_actions, ctx)

        # ── Sanity checks: cap solver sizing by SPR ──
        action = result["action"]
        amount = result.get("amount", 0)
        to_call = ctx.current_bet - ctx.my_bet_this_street
        spr = ctx.my_chips / max(ctx.pot, 1)

        # In high-SPR spots (deep stacks relative to pot), all-in is almost
        # never correct on flop/turn. Solver can't converge with 5k iterations
        # on such wide-ranging payoffs. Downgrade to a reasonable bet size.
        if action == "all_in" and spr > 8:
            # Replace all-in with a pot-sized raise (still aggressive but sane)
            if "raise" in valid_actions:
                result = self._make_raise(ctx, size_fraction=1.0)
            elif to_call <= 0 and "check" in valid_actions:
                result = {"action": "check", "amount": 0}

        # Cap raise overbets: no more than 2x pot in high-SPR spots
        if action == "raise" and amount > ctx.pot * 2.5 and spr > 5:
            result = self._make_raise(ctx, size_fraction=1.0)
        result["_iterations"] = iterations
        result["_probs"] = [round(p, 4) for p in probs]
        _log.warning(
            "POSTFLOP %s | cards=%s board=%s pot=%d bet=%d chips=%d | "
            "probs=%s | decision=%s amt=%s",
            ctx.street, ctx.hole_cards[:2], ctx.board, ctx.pot,
            ctx.current_bet, ctx.my_chips,
            [round(p, 3) for p in probs],
            result["action"], result.get("amount", 0),
        )
        return result

    def _advanced_preflop(self, ctx: GameContext, valid_actions: list[str]) -> dict:
        """Advanced preflop: tight-aggressive GTO-approximation using EHS."""
        from . import engine_bridge
        import logging
        _log = logging.getLogger("ai.advanced")

        r = _random_float()
        ehs = engine_bridge.compute_ehs(ctx.hole_cards[:2], ctx.board, 2000)
        to_call = ctx.current_bet - ctx.my_bet_this_street
        is_facing_raise = to_call > ctx.pot * 0.15
        is_facing_3bet = to_call > ctx.pot * 0.4

        if to_call <= 0:
            # Unopened — open raise ~top 40% (HU), check rest
            if ehs > 0.55 and "raise" in valid_actions:
                # Polarized sizing: bigger with premiums
                frac = 1.2 if ehs > 0.75 else 1.0
                return self._make_raise(ctx, size_fraction=frac)
            if ehs > 0.48 and "raise" in valid_actions and r < 0.3:
                return self._make_raise(ctx, size_fraction=1.0)
            return {"action": "check", "amount": 0} if "check" in valid_actions else {"action": "fold", "amount": 0}

        if is_facing_3bet:
            # Facing 3bet: continue with ~top 15%
            if ehs > 0.78:
                if r < 0.5 and "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=1.0)  # 4bet
                if "call" in valid_actions:
                    return {"action": "call", "amount": 0}
            if ehs > 0.68 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            # Occasional bluff 4bet
            if ehs > 0.58 and r < 0.08 and "raise" in valid_actions:
                return self._make_raise(ctx, size_fraction=1.0)
            return {"action": "fold", "amount": 0}

        if is_facing_raise:
            # Facing open raise: 3bet ~top 15%, call ~top 35%
            if ehs > 0.72:
                if r < 0.50 and "raise" in valid_actions:
                    return self._make_raise(ctx, size_fraction=1.0)  # 3bet value
                if "call" in valid_actions:
                    return {"action": "call", "amount": 0}
            if ehs > 0.55 and "call" in valid_actions:
                return {"action": "call", "amount": 0}
            # Bluff 3bet with some suited connectors
            if ehs > 0.50 and r < 0.10 and "raise" in valid_actions:
                return self._make_raise(ctx, size_fraction=1.0)
            return {"action": "fold", "amount": 0}

        # Facing limp
        if ehs > 0.55 and "raise" in valid_actions:
            return self._make_raise(ctx, size_fraction=1.0)
        if ehs > 0.45 and "call" in valid_actions:
            return {"action": "call", "amount": 0}
        return {"action": "fold", "amount": 0}

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
