"""Tests for the core game engine."""

import pytest
from game.deck import Deck, card_to_str, str_to_card, card_to_pretty
from game.evaluator import evaluate, hand_rank_string, compare_hands
from game.pot import calculate_pots, merge_pots, Pot
from game.stats import PlayerStats
from game.engine import GameEngine, Action, ActionType, Street


# ─── Deck tests ───────────────────────────────────────────────

class TestDeck:
    def test_deal_52_cards(self):
        deck = Deck()
        cards = deck.deal(52)
        assert len(cards) == 52
        assert len(set(cards)) == 52  # all unique

    def test_deal_too_many_raises(self):
        deck = Deck()
        deck.deal(50)
        with pytest.raises(ValueError):
            deck.deal(3)

    def test_shuffle_produces_different_order(self):
        d1 = Deck()
        order1 = d1.deal(52)
        d2 = Deck()
        order2 = d2.deal(52)
        # Extremely unlikely to be the same
        assert order1 != order2

    def test_card_conversion(self):
        for i in range(52):
            s = card_to_str(i)
            assert str_to_card(s) == i

    def test_card_pretty(self):
        c = str_to_card("As")
        assert "♠" in card_to_pretty(c)


# ─── Evaluator tests ─────────────────────────────────────────

class TestEvaluator:
    def test_royal_flush_beats_pair(self):
        # Royal flush: As Ks on board Qs Js Ts 2d 3c
        rf_hole = [str_to_card("As"), str_to_card("Ks")]
        pair_hole = [str_to_card("2h"), str_to_card("2c")]
        board = [
            str_to_card("Qs"), str_to_card("Js"), str_to_card("Ts"),
            str_to_card("7d"), str_to_card("3c"),
        ]
        rf_score = evaluate(rf_hole, board)
        pair_score = evaluate(pair_hole, board)
        assert rf_score < pair_score  # lower is better

    def test_hand_rank_string(self):
        hole = [str_to_card("As"), str_to_card("Ks")]
        board = [
            str_to_card("Qs"), str_to_card("Js"), str_to_card("Ts"),
            str_to_card("7d"), str_to_card("3c"),
        ]
        score = evaluate(hole, board)
        rank = hand_rank_string(score)
        assert "Flush" in rank  # Royal/Straight Flush

    def test_compare_hands(self):
        board = [
            str_to_card("Qs"), str_to_card("Js"), str_to_card("Ts"),
            str_to_card("7d"), str_to_card("3c"),
        ]
        holes = [
            [str_to_card("As"), str_to_card("Ks")],  # straight flush
            [str_to_card("2h"), str_to_card("2c")],  # pair
        ]
        result = compare_hands(holes, board)
        assert result[0][0] == 0  # player 0 wins


# ─── Pot tests ────────────────────────────────────────────────

class TestPot:
    def test_simple_pot(self):
        bets = {0: 100, 1: 100}
        pots = calculate_pots(bets, set())
        merged = merge_pots(pots)
        assert len(merged) == 1
        assert merged[0].amount == 200

    def test_side_pot(self):
        # Player 0 all-in for 50, players 1,2 bet 100
        bets = {0: 50, 1: 100, 2: 100}
        pots = merge_pots(calculate_pots(bets, set()))
        assert len(pots) == 2
        assert pots[0].amount == 150  # main: 50*3
        assert 0 in pots[0].eligible
        assert pots[1].amount == 100  # side: 50*2
        assert 0 not in pots[1].eligible

    def test_folded_player_contributes(self):
        bets = {0: 50, 1: 100, 2: 100}
        pots = merge_pots(calculate_pots(bets, {0}))
        # Player 0's 50 goes to pot but they can't win
        total = sum(p.amount for p in pots)
        assert total == 250
        for pot in pots:
            assert 0 not in pot.eligible


# ─── Stats tests ──────────────────────────────────────────────

class TestStats:
    def test_vpip(self):
        s = PlayerStats()
        s.record_hand(vpip=True, profit_bb=5.0)
        s.record_hand(vpip=False, profit_bb=-2.0)
        assert s.vpip == 50.0
        assert s.bb_per_hand == 1.5

    def test_empty_stats(self):
        s = PlayerStats()
        assert s.vpip == 0.0
        assert s.bb_per_hand == 0.0


# ─── Engine tests ─────────────────────────────────────────────

class TestEngine:
    def test_create_game(self):
        g = GameEngine(num_players=3)
        assert len(g.players) == 3
        assert g.street == Street.WAITING

    def test_start_hand(self):
        g = GameEngine(num_players=2)
        state = g.start_hand()
        assert state["street"] == "PREFLOP"
        assert len(state["board"]) == 0
        # Both players should have hole cards
        for p in g.players:
            assert len(p.hole_cards) == 2

    def test_heads_up_fold(self):
        """SB folds preflop, BB wins."""
        g = GameEngine(num_players=2, small_blind=1, big_blind=2)
        state = g.start_hand()
        # In heads-up, SB is button and acts first preflop
        state = g.act(Action(type=ActionType.FOLD))
        assert state["street"] == "WAITING"
        assert "result" in state
        # BB wins SB's blind
        assert state["result"]["winners"][0]["amount"] == 3  # SB(1) + BB(2)

    def test_check_through(self):
        """Both players check through a street."""
        g = GameEngine(num_players=2, small_blind=1, big_blind=2)
        g.start_hand()
        # Preflop: SB calls, BB checks
        g.act(Action(type=ActionType.CALL))
        state = g.act(Action(type=ActionType.CHECK))
        assert state["street"] == "FLOP"
        assert len(state["board"]) == 3

    def test_invalid_action_raises(self):
        g = GameEngine(num_players=2)
        g.start_hand()
        with pytest.raises(ValueError):
            g.act(Action(type=ActionType.CHECK))  # can't check when facing a bet

    def test_raise_and_call(self):
        g = GameEngine(num_players=2, small_blind=1, big_blind=2)
        g.start_hand()
        # SB raises to 6
        g.act(Action(type=ActionType.RAISE, amount=6))
        # BB calls
        state = g.act(Action(type=ActionType.CALL))
        assert state["street"] == "FLOP"

    def test_full_hand_to_showdown(self):
        """Play a complete hand to showdown with check-downs."""
        g = GameEngine(num_players=2, small_blind=1, big_blind=2)
        g.start_hand()
        # Preflop: call, check
        g.act(Action(type=ActionType.CALL))
        g.act(Action(type=ActionType.CHECK))
        # Flop: check, check
        g.act(Action(type=ActionType.CHECK))
        g.act(Action(type=ActionType.CHECK))
        # Turn: check, check
        g.act(Action(type=ActionType.CHECK))
        g.act(Action(type=ActionType.CHECK))
        # River: check, check
        g.act(Action(type=ActionType.CHECK))
        state = g.act(Action(type=ActionType.CHECK))
        assert state["street"] == "WAITING"
        assert "result" in state
        assert len(state["result"]["board"]) == 5

    def test_three_player_game(self):
        g = GameEngine(num_players=3, small_blind=1, big_blind=2)
        state = g.start_hand()
        assert g.num_players == 3
        # UTG folds
        g.act(Action(type=ActionType.FOLD))
        # SB calls
        g.act(Action(type=ActionType.CALL))
        # BB checks
        state = g.act(Action(type=ActionType.CHECK))
        assert state["street"] == "FLOP"

    def test_player_count_validation(self):
        with pytest.raises(ValueError):
            GameEngine(num_players=1)
        with pytest.raises(ValueError):
            GameEngine(num_players=7)

    def test_stats_update_after_hand(self):
        g = GameEngine(num_players=2, small_blind=1, big_blind=2)
        g.start_hand()
        g.act(Action(type=ActionType.FOLD))
        for p in g.players:
            assert p.stats.hands_played == 1

    def test_multiple_hands(self):
        g = GameEngine(num_players=2, small_blind=1, big_blind=2)
        for _ in range(5):
            g.start_hand()
            g.act(Action(type=ActionType.FOLD))
        assert g.hand_number == 5
        for p in g.players:
            assert p.stats.hands_played == 5
