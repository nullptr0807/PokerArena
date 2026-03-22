"""Bridge between Python AI agent and C++ MCCFR engine.

Loads the compiled pybind11 module and provides a Pythonic interface
for querying blueprint strategies and running subgame solving.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Optional

# Add the C++ build dir to path so we can import poker_ai_py
_BUILD_DIR = Path(__file__).parent / "cpp" / "build"
if _BUILD_DIR.exists() and str(_BUILD_DIR) not in sys.path:
    sys.path.insert(0, str(_BUILD_DIR))

_engine_available = False
_trainer = None
_solver = None

try:
    import poker_ai_py  # type: ignore

    _engine_available = True
    # Create trainer + solver immediately (solver works without blueprint)
    _trainer = poker_ai_py.MCCFRTrainer(2)

    # Auto-load blueprint from known locations
    _blueprint_candidates = [
        Path(__file__).parent / "blueprint.bin",          # backend/ai/blueprint.bin
        _BUILD_DIR / "blueprint.bin",                     # backend/ai/cpp/build/blueprint.bin
        _BUILD_DIR / "data" / "blueprint.bin",            # backend/ai/cpp/build/data/blueprint.bin
    ]
    for _bp_path in _blueprint_candidates:
        if _bp_path.exists():
            _trainer.load(str(_bp_path))
            if _trainer.num_info_sets() > 0:
                break

    _solver = poker_ai_py.SubgameSolver(_trainer, 4)
except ImportError:
    poker_ai_py = None


def is_available() -> bool:
    """Check if the C++ engine is compiled and importable."""
    return _engine_available


def load_blueprint(path: str | Path | None = None) -> bool:
    """Load a pre-trained blueprint strategy.

    Args:
        path: Path to the blueprint binary. If None, looks for
              'blueprint.bin' in the cpp/build directory.
    Returns:
        True if loaded successfully.
    """
    global _trainer, _solver
    if not _engine_available:
        return False

    if path is None:
        path = _BUILD_DIR / "blueprint.bin"
    path = Path(path)

    _trainer = poker_ai_py.MCCFRTrainer(2)
    if path.exists():
        _trainer.load(str(path))
        _solver = poker_ai_py.SubgameSolver(_trainer, 4)
        return _trainer.num_info_sets() > 0
    return False


def query_blueprint(info_set_key: str) -> list[float]:
    """Query the blueprint strategy for an information set."""
    if _trainer is None:
        return [1.0 / 6] * 6
    return list(_trainer.query(info_set_key))


def compute_ehs(hole: list[int], board: list[int], rollouts: int = 1000) -> float:
    """Compute Expected Hand Strength via C++ Monte Carlo."""
    if not _engine_available:
        return 0.5
    return poker_ai_py.compute_ehs(hole, board, rollouts)


def solve_subgame(
    hole_cards: list[int],
    board: list[int],
    pot: int,
    current_bet: int,
    my_chips: int,
    opp_chips: int,
    iterations: int = 5000,
) -> list[float]:
    """Run real-time subgame solving and return action probabilities.

    Returns:
        List of 6 probabilities: [fold, check/call, half-pot, pot, 2x-pot, all-in]
    """
    if _solver is None or not _engine_available:
        return [1.0 / 6] * 6

    state = poker_ai_py.SubgameState()
    state.hole_cards = hole_cards[:2]
    state.board = board
    state.pot = pot
    state.current_bet = current_bet
    state.my_chips = my_chips
    state.opp_chips = opp_chips
    state.action_history = ""

    result = _solver.solve(state, iterations)
    return list(result)


# Action index mapping
ACTION_NAMES = ["fold", "check_call", "bet_half", "bet_pot", "bet_2x", "all_in"]


def pick_action(probs: list[float], valid_actions: list[str], ctx=None) -> dict:
    """Convert C++ action probabilities to a game action.

    Maps the 6-action abstracted space back to the game's action set.
    """
    import secrets

    # Map game actions to C++ indices
    action_map = {
        "fold": 0,
        "check": 1,
        "call": 1,
        "raise": [2, 3, 4],  # raise maps to half-pot, pot, 2x
        "all_in": 5,
    }

    # Filter to valid actions and their probs
    valid_probs = []
    for action in valid_actions:
        idx = action_map.get(action, -1)
        if isinstance(idx, list):
            # For raise, sum all raise-size probs
            total_raise = sum(probs[i] for i in idx)
            valid_probs.append((action, total_raise))
        elif idx >= 0:
            valid_probs.append((action, probs[idx]))

    if not valid_probs:
        return {"action": "fold", "amount": 0}

    # Normalize
    total = sum(p for _, p in valid_probs)
    if total <= 0:
        chosen = valid_probs[secrets.randbelow(len(valid_probs))]
        return {"action": chosen[0], "amount": 0}

    # Sample from distribution
    r = secrets.randbelow(10000) / 10000.0
    cumul = 0.0
    chosen_action = valid_probs[-1][0]
    for action, prob in valid_probs:
        cumul += prob / total
        if r < cumul:
            chosen_action = action
            break

    # Calculate raise amount if needed
    amount = 0
    if chosen_action == "raise" and ctx is not None:
        # Pick raise size based on sub-probabilities
        raise_probs = [probs[2], probs[3], probs[4]]
        raise_total = sum(raise_probs)
        if raise_total > 0:
            r2 = secrets.randbelow(10000) / 10000.0
            c2 = 0.0
            frac = 0.5  # default half pot
            for i, (f, p) in enumerate(zip([0.5, 1.0, 2.0], raise_probs)):
                c2 += p / raise_total
                if r2 < c2:
                    frac = f
                    break
            pot = ctx.pot if hasattr(ctx, 'pot') else 0
            current_bet = ctx.current_bet if hasattr(ctx, 'current_bet') else 0
            my_bet = ctx.my_bet_this_street if hasattr(ctx, 'my_bet_this_street') else 0
            amount = int(pot * frac) + current_bet
            min_raise = current_bet * 2
            amount = max(amount, min_raise)
            # Cap at all-in
            max_amount = ctx.my_chips + my_bet if hasattr(ctx, 'my_chips') else amount
            amount = min(amount, max_amount)

    return {"action": chosen_action, "amount": amount}
