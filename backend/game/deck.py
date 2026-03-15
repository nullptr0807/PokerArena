"""Cryptographically secure deck with Fisher-Yates shuffle."""

import secrets
from dataclasses import dataclass, field

# Standard 52-card deck represented as integers 0-51
# Suits: 0=spades, 1=hearts, 2=diamonds, 3=clubs
# Ranks: 0=2, 1=3, ..., 8=T, 9=J, 10=Q, 11=K, 12=A

RANK_NAMES = "23456789TJQKA"
SUIT_NAMES = "shdc"
SUIT_SYMBOLS = {"s": "♠", "h": "♥", "d": "♦", "c": "♣"}


def card_to_str(card: int) -> str:
    """Convert card int (0-51) to human-readable string like 'As', 'Td'."""
    rank = card % 13
    suit = card // 13
    return f"{RANK_NAMES[rank]}{SUIT_NAMES[suit]}"


def card_to_pretty(card: int) -> str:
    """Convert card int to pretty string like 'A♠'."""
    rank = card % 13
    suit = card // 13
    return f"{RANK_NAMES[rank]}{SUIT_SYMBOLS[SUIT_NAMES[suit]]}"


def str_to_card(s: str) -> int:
    """Convert 'As' style string to card int."""
    rank = RANK_NAMES.index(s[0])
    suit = SUIT_NAMES.index(s[1])
    return suit * 13 + rank


class Deck:
    """A shuffled deck of 52 cards using cryptographically secure RNG."""

    def __init__(self) -> None:
        self._cards: list[int] = list(range(52))
        self._index: int = 0
        self.shuffle()

    def shuffle(self) -> None:
        """Fisher-Yates shuffle with secrets.randbelow for fairness."""
        self._index = 0
        cards = self._cards
        for i in range(51, 0, -1):
            j = secrets.randbelow(i + 1)
            cards[i], cards[j] = cards[j], cards[i]

    def deal(self, n: int = 1) -> list[int]:
        """Deal n cards from the top."""
        if self._index + n > 52:
            raise ValueError("Not enough cards in deck")
        dealt = self._cards[self._index : self._index + n]
        self._index += n
        return dealt

    def deal_one(self) -> int:
        """Deal a single card."""
        return self.deal(1)[0]

    @property
    def remaining(self) -> int:
        return 52 - self._index
