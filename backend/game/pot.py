"""Pot calculation with side pot support."""

from __future__ import annotations
from dataclasses import dataclass


@dataclass
class Pot:
    """A single pot (main or side) with eligible players."""
    amount: int  # total chips in this pot
    eligible: set[int]  # player indices eligible to win this pot


def calculate_pots(bets: dict[int, int], folded: set[int]) -> list[Pot]:
    """
    Calculate main pot and side pots from player bets.
    
    Args:
        bets: {player_index: total_bet_this_hand} for ALL players (including folded/all-in)
        folded: set of player indices who have folded
    
    Returns:
        List of Pot objects (main pot first, then side pots)
    
    Example:
        Player 0 bets 100, Player 1 bets 200, Player 2 bets 200, Player 3 folded at 50
        bets = {0: 100, 1: 200, 2: 200, 3: 50}
        folded = {3}
        Result:
          Pot(amount=200, eligible={0, 1, 2})   # main pot: 50*4=200 (but 3 folded partial)
          ...adjusted for actual contributions
    """
    if not bets:
        return []

    # Get unique sorted bet levels
    active_bets = {pid: amt for pid, amt in bets.items() if amt > 0}
    if not active_bets:
        return []

    # Sort players by their bet amount (ascending) for side pot calculation
    sorted_players = sorted(active_bets.items(), key=lambda x: x[1])
    
    pots: list[Pot] = []
    prev_level = 0

    for i, (pid, bet_amount) in enumerate(sorted_players):
        level = bet_amount
        if level <= prev_level:
            continue
        
        # How much each contributing player adds to this pot layer
        layer = level - prev_level
        
        # All players who bet >= this level contribute
        contributors = 0
        eligible = set()
        for other_pid, other_bet in active_bets.items():
            if other_bet >= level:
                contributors += 1
                contribution = layer
            else:
                contribution = max(0, other_bet - prev_level)
            
            if contribution > 0:
                if other_pid not in folded:
                    eligible.add(other_pid)
                # Even folded players contribute chips, they just can't win
        
        # Calculate total for this layer
        total = 0
        for other_pid, other_bet in active_bets.items():
            if other_bet >= level:
                total += layer
            else:
                c = max(0, other_bet - prev_level)
                total += c
        
        if total > 0 and eligible:
            pots.append(Pot(amount=total, eligible=eligible))
        elif total > 0 and not eligible:
            # Dead money — everyone eligible folded, add to next pot or last pot
            if pots:
                pots[-1].amount += total
            # else: edge case, all players folded (shouldn't happen)
        
        prev_level = level

    return pots


def merge_pots(pots: list[Pot]) -> list[Pot]:
    """Merge consecutive pots with identical eligible sets."""
    if not pots:
        return []
    merged = [Pot(amount=pots[0].amount, eligible=set(pots[0].eligible))]
    for pot in pots[1:]:
        if pot.eligible == merged[-1].eligible:
            merged[-1].amount += pot.amount
        else:
            merged.append(Pot(amount=pot.amount, eligible=set(pot.eligible)))
    return merged
