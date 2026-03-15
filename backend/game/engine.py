"""
Texas Hold'em No-Limit Cash Game engine.

State machine: WAITING -> PREFLOP -> FLOP -> TURN -> RIVER -> SHOWDOWN -> WAITING
"""

from __future__ import annotations

import secrets
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable

from .deck import Deck, card_to_str
from .evaluator import compare_hands, hand_rank_string, evaluate
from .pot import Pot, calculate_pots, merge_pots
from .stats import PlayerStats


class Street(Enum):
    WAITING = auto()
    PREFLOP = auto()
    FLOP = auto()
    TURN = auto()
    RIVER = auto()
    SHOWDOWN = auto()


class ActionType(Enum):
    FOLD = "fold"
    CHECK = "check"
    CALL = "call"
    RAISE = "raise"      # includes bet (first raise on a street)
    ALL_IN = "all_in"


@dataclass
class Action:
    type: ActionType
    amount: int = 0  # total chips put in this street (for raise/all_in)


@dataclass
class Player:
    index: int
    name: str
    chips: int
    is_human: bool = False
    hole_cards: list[int] = field(default_factory=list)
    bet_this_street: int = 0    # chips bet in current street
    bet_this_hand: int = 0      # total chips bet in current hand
    folded: bool = False
    all_in: bool = False
    acted_this_street: bool = False
    stats: PlayerStats = field(default_factory=PlayerStats)

    @property
    def active(self) -> bool:
        """Can still act (not folded, not all-in)."""
        return not self.folded and not self.all_in

    @property
    def in_hand(self) -> bool:
        """Still in the hand (not folded)."""
        return not self.folded

    def reset_for_hand(self) -> None:
        self.hole_cards = []
        self.bet_this_street = 0
        self.bet_this_hand = 0
        self.folded = False
        self.all_in = False
        self.acted_this_street = False

    def reset_for_street(self) -> None:
        self.bet_this_street = 0
        self.acted_this_street = False


@dataclass
class HandResult:
    """Result of a completed hand."""
    pots: list[Pot]
    winners: list[dict]  # [{player_index, amount_won, hand_rank}]
    board: list[int]
    run_it_times: int = 1


class GameEngine:
    """
    Core Texas Hold'em engine managing a single table.
    
    Supports 2-6 players, no-limit cash game with fixed blinds.
    """

    def __init__(
        self,
        num_players: int,
        small_blind: int = 1,
        big_blind: int = 2,
        starting_chips: int = 400,  # 200BB at 1/2
    ):
        if not 2 <= num_players <= 6:
            raise ValueError("num_players must be 2-6")

        self.small_blind = small_blind
        self.big_blind = big_blind
        self.deck = Deck()
        self.street = Street.WAITING
        self.board: list[int] = []
        self.action_log: list[dict] = []  # log of all actions this hand

        # Players
        self.players: list[Player] = []
        for i in range(num_players):
            self.players.append(
                Player(
                    index=i,
                    name=f"Player {i}",
                    chips=starting_chips,
                    is_human=(i == 0),
                )
            )

        # Dealer button position (rotates each hand)
        self.button: int = 0
        self.current_player: int = 0
        self.last_raiser: int = -1
        self.current_bet: int = 0  # current highest bet this street
        self.min_raise: int = big_blind  # minimum raise increment
        self.hand_number: int = 0

        # All-in / run it multiple times
        self.all_in_showdown: bool = False

    @property
    def num_active(self) -> int:
        """Players who can still act."""
        return sum(1 for p in self.players if p.active)

    @property
    def num_in_hand(self) -> int:
        """Players still in hand (not folded)."""
        return sum(1 for p in self.players if p.in_hand)

    @property
    def num_players(self) -> int:
        return len(self.players)

    # ─── Hand lifecycle ───────────────────────────────────────────

    def start_hand(self) -> dict:
        """Start a new hand. Returns initial state."""
        self.hand_number += 1
        self.deck.shuffle()
        self.board = []
        self.action_log = []
        self.all_in_showdown = False
        self.street = Street.PREFLOP

        # Reset players
        for p in self.players:
            p.reset_for_hand()

        # Post blinds
        sb_idx = self._next_active_from(self.button, count=1)
        bb_idx = self._next_active_from(self.button, count=2)

        self._post_blind(sb_idx, self.small_blind)
        self._post_blind(bb_idx, self.big_blind)

        self.current_bet = self.big_blind
        self.min_raise = self.big_blind
        self.last_raiser = bb_idx

        # Deal hole cards
        for p in self.players:
            p.hole_cards = self.deck.deal(2)

        # Action starts left of BB (UTG)
        self.current_player = self._next_active_from(bb_idx)

        return self.get_state()

    def _post_blind(self, player_idx: int, amount: int) -> None:
        p = self.players[player_idx]
        actual = min(amount, p.chips)
        p.chips -= actual
        p.bet_this_street = actual
        p.bet_this_hand = actual
        if p.chips == 0:
            p.all_in = True
        self.action_log.append({
            "player": player_idx,
            "action": "blind",
            "amount": actual,
        })

    # ─── Player actions ───────────────────────────────────────────

    def get_valid_actions(self, player_idx: int | None = None) -> list[ActionType]:
        """Get valid actions for the current (or specified) player."""
        idx = player_idx if player_idx is not None else self.current_player
        p = self.players[idx]

        if not p.active or self.street in (Street.WAITING, Street.SHOWDOWN):
            return []

        actions = [ActionType.FOLD]
        to_call = self.current_bet - p.bet_this_street

        if to_call <= 0:
            actions.append(ActionType.CHECK)
        else:
            if to_call >= p.chips:
                # Can only fold or go all-in (call would be all-in)
                actions.append(ActionType.ALL_IN)
                return actions
            actions.append(ActionType.CALL)

        # Can raise if chips allow
        min_raise_to = self.current_bet + self.min_raise
        if p.chips + p.bet_this_street > self.current_bet:
            if p.chips + p.bet_this_street >= min_raise_to:
                actions.append(ActionType.RAISE)
            actions.append(ActionType.ALL_IN)

        return actions

    def act(self, action: Action) -> dict:
        """
        Execute an action for the current player.
        
        Returns the new game state dict.
        Raises ValueError on invalid actions.
        """
        p = self.players[self.current_player]
        valid = self.get_valid_actions()

        if action.type not in valid:
            raise ValueError(
                f"Invalid action {action.type.value} for player {self.current_player}. "
                f"Valid: {[a.value for a in valid]}"
            )

        log_entry = {"player": self.current_player, "action": action.type.value}

        if action.type == ActionType.FOLD:
            p.folded = True

        elif action.type == ActionType.CHECK:
            pass

        elif action.type == ActionType.CALL:
            to_call = self.current_bet - p.bet_this_street
            actual = min(to_call, p.chips)
            p.chips -= actual
            p.bet_this_street += actual
            p.bet_this_hand += actual
            if p.chips == 0:
                p.all_in = True
            log_entry["amount"] = actual

        elif action.type == ActionType.RAISE:
            # action.amount is the TOTAL bet this street (not the raise increment)
            raise_to = action.amount
            min_raise_to = self.current_bet + self.min_raise
            max_raise_to = p.chips + p.bet_this_street  # all-in

            if raise_to < min_raise_to:
                raise ValueError(
                    f"Raise to {raise_to} below minimum {min_raise_to}"
                )
            if raise_to > max_raise_to:
                raise ValueError(
                    f"Raise to {raise_to} exceeds max {max_raise_to}"
                )

            raise_increment = raise_to - self.current_bet
            cost = raise_to - p.bet_this_street
            p.chips -= cost
            p.bet_this_street = raise_to
            p.bet_this_hand += cost
            self.current_bet = raise_to
            self.min_raise = max(self.min_raise, raise_increment)
            self.last_raiser = self.current_player
            if p.chips == 0:
                p.all_in = True
            log_entry["amount"] = raise_to

        elif action.type == ActionType.ALL_IN:
            cost = p.chips
            p.bet_this_street += cost
            p.bet_this_hand += cost
            total_bet = p.bet_this_street
            if total_bet > self.current_bet:
                raise_increment = total_bet - self.current_bet
                if raise_increment >= self.min_raise:
                    self.min_raise = raise_increment
                    self.last_raiser = self.current_player
                self.current_bet = total_bet
            p.chips = 0
            p.all_in = True
            log_entry["amount"] = total_bet

        p.acted_this_street = True
        self.action_log.append(log_entry)

        # Advance game
        return self._advance()

    # ─── Game flow ────────────────────────────────────────────────

    def _advance(self) -> dict:
        """Advance game state after an action."""
        # Check if only one player remains
        if self.num_in_hand == 1:
            return self._award_last_standing()

        # Check if betting round is complete
        if self._is_street_complete():
            return self._next_street()

        # Move to next active player
        self.current_player = self._next_active_from(self.current_player)
        return self.get_state()

    def _is_street_complete(self) -> bool:
        """Check if all active players have acted and bets are matched."""
        for p in self.players:
            if not p.active:
                continue
            if not p.acted_this_street:
                return False
            if p.bet_this_street != self.current_bet and not p.all_in:
                return False
        return True

    def _next_street(self) -> dict:
        """Transition to the next street."""
        # Reset per-street state
        for p in self.players:
            p.reset_for_street()
        self.current_bet = 0
        self.min_raise = self.big_blind
        self.last_raiser = -1

        if self.street == Street.PREFLOP:
            self.street = Street.FLOP
            self.deck.deal_one()  # burn
            self.board.extend(self.deck.deal(3))
        elif self.street == Street.FLOP:
            self.street = Street.TURN
            self.deck.deal_one()  # burn
            self.board.append(self.deck.deal_one())
        elif self.street == Street.TURN:
            self.street = Street.RIVER
            self.deck.deal_one()  # burn
            self.board.append(self.deck.deal_one())
        elif self.street == Street.RIVER:
            return self._showdown()

        # Check if everyone is all-in (no more action possible)
        if self.num_active <= 1:
            self.all_in_showdown = True
            return self._run_out_board()

        # First to act post-flop: first active player left of button
        self.current_player = self._next_active_from(self.button)
        return self.get_state()

    def _run_out_board(self) -> dict:
        """Deal remaining community cards when all players are all-in."""
        while len(self.board) < 5:
            self.deck.deal_one()  # burn
            self.board.append(self.deck.deal_one())
        return self._showdown()

    def _showdown(self, run_count: int = 1) -> dict:
        """
        Determine winners and distribute pots.
        
        Args:
            run_count: number of times to run out the board (for Run It Multiple Times)
        """
        self.street = Street.SHOWDOWN
        
        # Calculate pots
        bets = {p.index: p.bet_this_hand for p in self.players}
        folded = {p.index for p in self.players if p.folded}
        pots = merge_pots(calculate_pots(bets, folded))

        # Determine winners for each pot
        winners: list[dict] = []
        in_hand_holes = [
            (p.index, p.hole_cards) for p in self.players if p.in_hand
        ]

        for pot in pots:
            eligible_holes = [
                (idx, cards) for idx, cards in in_hand_holes if idx in pot.eligible
            ]
            if len(eligible_holes) == 1:
                idx = eligible_holes[0][0]
                score = evaluate(eligible_holes[0][1], self.board)
                winners.append({
                    "player": idx,
                    "amount": pot.amount,
                    "hand_rank": hand_rank_string(score),
                })
                self.players[idx].chips += pot.amount
            else:
                # Compare hands
                rankings = compare_hands(
                    [cards for _, cards in eligible_holes], self.board
                )
                # Find all players tied for best
                best_score = rankings[0][1]
                pot_winners = [
                    eligible_holes[r[0]][0]
                    for r in rankings
                    if r[1] == best_score
                ]
                share = pot.amount // len(pot_winners)
                remainder = pot.amount % len(pot_winners)
                for i, widx in enumerate(pot_winners):
                    won = share + (1 if i < remainder else 0)
                    winners.append({
                        "player": widx,
                        "amount": won,
                        "hand_rank": hand_rank_string(best_score),
                    })
                    self.players[widx].chips += won

        # Update stats
        for p in self.players:
            # VPIP: voluntarily put money in (not just blinds)
            vpip = p.bet_this_hand > self.big_blind or (
                p.bet_this_hand == self.big_blind
                and any(
                    log["player"] == p.index and log["action"] in ("call", "raise", "all_in")
                    for log in self.action_log
                )
            )
            initial_chips_this_hand = p.chips + p.bet_this_hand - sum(
                w["amount"] for w in winners if w["player"] == p.index
            )
            profit_bb = (p.chips - initial_chips_this_hand) / self.big_blind
            # Simpler: profit = winnings - amount_bet
            winnings = sum(w["amount"] for w in winners if w["player"] == p.index)
            profit_bb = (winnings - p.bet_this_hand) / self.big_blind
            p.stats.record_hand(vpip, profit_bb)

        # Advance button
        self.button = self._next_player_from(self.button)
        self.street = Street.WAITING

        result = HandResult(pots=pots, winners=winners, board=list(self.board))
        return self._state_with_result(result)

    def _award_last_standing(self) -> dict:
        """Award pot to last remaining player (everyone else folded)."""
        self.street = Street.SHOWDOWN
        winner = next(p for p in self.players if p.in_hand)
        
        total_pot = sum(p.bet_this_hand for p in self.players)
        winner.chips += total_pot

        # Update stats
        for p in self.players:
            vpip = p.bet_this_hand > self.big_blind or (
                p.bet_this_hand == self.big_blind
                and any(
                    log["player"] == p.index and log["action"] in ("call", "raise", "all_in")
                    for log in self.action_log
                )
            )
            winnings = total_pot if p.index == winner.index else 0
            profit_bb = (winnings - p.bet_this_hand) / self.big_blind
            p.stats.record_hand(vpip, profit_bb)

        self.button = self._next_player_from(self.button)
        self.street = Street.WAITING

        result = HandResult(
            pots=[Pot(amount=total_pot, eligible={winner.index})],
            winners=[{"player": winner.index, "amount": total_pot, "hand_rank": ""}],
            board=list(self.board),
        )
        return self._state_with_result(result)

    # ─── Run it multiple times ────────────────────────────────────

    def run_it_multiple(self, times: int = 1) -> list[dict]:
        """
        Run the remaining board multiple times for all-in situations.
        Can only be called when all_in_showdown is True and board < 5 cards.
        
        Returns list of showdown results, one per run.
        """
        if not self.all_in_showdown:
            raise ValueError("Run it multiple times only available when all-in")
        if times not in (1, 2, 3):
            raise ValueError("Can only run 1-3 times")
        if len(self.board) >= 5:
            raise ValueError("Board already complete")

        # Save state
        saved_board = list(self.board)
        saved_chips = {p.index: p.chips for p in self.players}
        saved_deck_idx = self.deck._index

        results = []
        for _ in range(times):
            # Reset to saved state
            self.board = list(saved_board)
            self.deck._index = saved_deck_idx
            for p in self.players:
                p.chips = saved_chips[p.index]

            # Re-shuffle remaining cards for each run
            remaining_start = self.deck._index
            remaining = self.deck._cards[remaining_start:]
            for i in range(len(remaining) - 1, 0, -1):
                j = secrets.randbelow(i + 1)
                remaining[i], remaining[j] = remaining[j], remaining[i]
            self.deck._cards[remaining_start:] = remaining

            result = self._run_out_board()
            results.append(result)

        # Average the chips across runs
        avg_chips: dict[int, float] = {p.index: 0 for p in self.players}
        for r in results:
            for p in self.players:
                avg_chips[p.index] += p.chips
        
        # Restore and set averaged chips
        self.board = saved_board  
        for p in self.players:
            p.chips = round(avg_chips[p.index] / times)

        self.button = self._next_player_from(self.button)
        self.street = Street.WAITING
        return results

    # ─── Helpers ──────────────────────────────────────────────────

    def _next_player_from(self, pos: int, count: int = 1) -> int:
        """Find the nth next player (seat, regardless of fold/all-in)."""
        idx = pos
        for _ in range(count):
            idx = (idx + 1) % self.num_players
        return idx

    def _next_active_from(self, pos: int, count: int = 1) -> int:
        """Find the nth next active (can act) player from position."""
        idx = pos
        found = 0
        for _ in range(self.num_players * count):
            idx = (idx + 1) % self.num_players
            if self.players[idx].active:
                found += 1
                if found == count:
                    return idx
        # Fallback: return next in-hand player
        idx = pos
        found = 0
        for _ in range(self.num_players * count):
            idx = (idx + 1) % self.num_players
            if self.players[idx].in_hand:
                found += 1
                if found == count:
                    return idx
        return (pos + 1) % self.num_players

    # ─── State serialization ─────────────────────────────────────

    def get_state(self, reveal_all: bool = False) -> dict:
        """
        Get current game state as dict.
        
        By default, only the human player's hole cards are revealed.
        Set reveal_all=True to see all cards (for debugging or showdown).
        """
        players = []
        for p in self.players:
            pdata = {
                "index": p.index,
                "name": p.name,
                "chips": p.chips,
                "bet_this_street": p.bet_this_street,
                "bet_this_hand": p.bet_this_hand,
                "folded": p.folded,
                "all_in": p.all_in,
                "is_human": p.is_human,
                "stats": p.stats.to_dict(),
            }
            if reveal_all or p.is_human:
                pdata["hole_cards"] = [card_to_str(c) for c in p.hole_cards]
            else:
                pdata["hole_cards"] = None
            players.append(pdata)

        return {
            "hand_number": self.hand_number,
            "street": self.street.name,
            "board": [card_to_str(c) for c in self.board],
            "current_player": self.current_player,
            "current_bet": self.current_bet,
            "pot": sum(p.bet_this_hand for p in self.players),
            "players": players,
            "valid_actions": [a.value for a in self.get_valid_actions()],
            "min_raise_to": self.current_bet + self.min_raise,
            "button": self.button,
            "all_in_showdown": self.all_in_showdown,
        }

    def _state_with_result(self, result: HandResult) -> dict:
        """Get state with showdown result included."""
        state = self.get_state(reveal_all=True)
        state["result"] = {
            "winners": result.winners,
            "board": [card_to_str(c) for c in result.board],
            "pots": [{"amount": p.amount, "eligible": list(p.eligible)} for p in result.pots],
        }
        return state
