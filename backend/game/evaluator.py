"""Hand evaluator wrapping the treys library."""

from treys import Card as TreysCard
from treys import Evaluator as TreysEvaluator

from .deck import RANK_NAMES, SUIT_NAMES

_evaluator = TreysEvaluator()

# Treys rank class names
HAND_RANKS = [
    "Royal Flush",  # 0
    "Straight Flush",
    "Four of a Kind",
    "Full House",
    "Flush",
    "Straight",
    "Three of a Kind",
    "Two Pair",
    "Pair",
    "High Card",
]


def _to_treys(card: int) -> int:
    """Convert our card int (0-51) to treys card int."""
    rank = card % 13
    suit = card // 13
    rank_char = RANK_NAMES[rank]
    suit_char = SUIT_NAMES[suit]
    return TreysCard.new(f"{rank_char}{suit_char}")


def evaluate(hole_cards: list[int], board: list[int]) -> int:
    """
    Evaluate a hand. Returns a score where LOWER is BETTER (1 = Royal Flush).
    
    Args:
        hole_cards: list of 2 card ints (player's hole cards)
        board: list of 3-5 card ints (community cards)
    
    Returns:
        int score (1 = best, 7462 = worst)
    """
    treys_hole = [_to_treys(c) for c in hole_cards]
    treys_board = [_to_treys(c) for c in board]
    return _evaluator.evaluate(treys_board, treys_hole)


def hand_rank_string(score: int) -> str:
    """Get human-readable hand rank from score."""
    rank_class = _evaluator.get_rank_class(score)
    return HAND_RANKS[rank_class] if rank_class < len(HAND_RANKS) else "Unknown"


def compare_hands(
    players_holes: list[list[int]], board: list[int]
) -> list[tuple[int, int]]:
    """
    Compare multiple players' hands on the same board.
    
    Returns:
        List of (player_index, score) sorted best to worst (lowest score first).
    """
    results = []
    for i, hole in enumerate(players_holes):
        score = evaluate(hole, board)
        results.append((i, score))
    results.sort(key=lambda x: x[1])
    return results
