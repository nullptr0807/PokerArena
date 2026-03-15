"""Player statistics tracking."""

from __future__ import annotations
from dataclasses import dataclass, field


@dataclass
class PlayerStats:
    """Track per-player statistics across a session."""
    
    hands_played: int = 0
    hands_vpip: int = 0          # hands where player voluntarily put money in pot
    total_profit_bb: float = 0.0  # cumulative profit in big blinds
    
    @property
    def vpip(self) -> float:
        """VPIP percentage (0-100)."""
        if self.hands_played == 0:
            return 0.0
        return (self.hands_vpip / self.hands_played) * 100.0
    
    @property
    def bb_per_hand(self) -> float:
        """Average profit per hand in big blinds."""
        if self.hands_played == 0:
            return 0.0
        return self.total_profit_bb / self.hands_played
    
    def record_hand(self, vpip: bool, profit_bb: float) -> None:
        """Record the result of one hand."""
        self.hands_played += 1
        if vpip:
            self.hands_vpip += 1
        self.total_profit_bb += profit_bb
    
    def to_dict(self) -> dict:
        return {
            "hands_played": self.hands_played,
            "vpip": round(self.vpip, 1),
            "bb_per_hand": round(self.bb_per_hand, 2),
            "total_profit_bb": round(self.total_profit_bb, 2),
        }
