"""WebSocket handler for real-time poker game communication."""

from __future__ import annotations

import asyncio
import json
import time
from typing import Any

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from game.engine import GameEngine, Action, ActionType, Street
from ai.agent import AIAgent, Difficulty, GameContext

router = APIRouter()

TURN_TIMEOUT = 30  # seconds


class GameRoom:
    """Manages a single poker game room with WebSocket communication."""

    def __init__(
        self,
        num_players: int = 2,
        small_blind: int = 1,
        big_blind: int = 2,
        starting_chips: int = 400,
        ai_difficulties: list[str] | None = None,
    ) -> None:
        self.engine = GameEngine(
            num_players=num_players,
            small_blind=small_blind,
            big_blind=big_blind,
            starting_chips=starting_chips,
        )
        # Set player names
        self.engine.players[0].name = "You"
        for i in range(1, num_players):
            self.engine.players[i].name = f"AI {i}"

        # Create AI agents with specified difficulties
        difficulties = ai_difficulties or ["normal"] * (num_players - 1)
        self.ai_agents: list[AIAgent] = []
        for diff_str in difficulties:
            diff = Difficulty(diff_str)
            self.ai_agents.append(AIAgent(difficulty=diff))

        self.ws: WebSocket | None = None
        self._turn_deadline: float = 0
        self._timeout_task: asyncio.Task | None = None
        self.action_log: list[dict] = []

    async def send(self, data: dict) -> None:
        """Send JSON message to the connected client."""
        if self.ws:
            await self.ws.send_json(data)

    def _append_action(self, player: int, name: str, action: str, amount: int) -> None:
        """Append a player action to the action log."""
        self.action_log.append({
            "player": player,
            "name": name,
            "action": action,
            "amount": amount,
            "street": self.engine.street.name,
            "timestamp": time.time(),
        })

    def _append_street(self) -> None:
        """Append a street transition to the action log."""
        from game.engine import card_to_str
        self.action_log.append({
            "type": "street",
            "street": self.engine.street.name,
            "board": [card_to_str(c) for c in self.engine.board],
        })

    async def _broadcast_action_log(self) -> None:
        """Broadcast current action log to client."""
        await self.send({"type": "action_log", "data": self.action_log})

    def _inject_action_log(self, state: dict) -> dict:
        """Add action_log to state dict."""
        state["action_log"] = self.action_log
        return state

    def _start_turn_timer(self) -> None:
        """Start a 30s countdown for the current player."""
        self._cancel_turn_timer()
        self._turn_deadline = time.time() + TURN_TIMEOUT
        self._timeout_task = asyncio.create_task(self._turn_timeout())

    def _cancel_turn_timer(self) -> None:
        if self._timeout_task and not self._timeout_task.done():
            self._timeout_task.cancel()
        self._timeout_task = None

    async def _turn_timeout(self) -> None:
        """Auto-fold when timer expires."""
        try:
            await asyncio.sleep(TURN_TIMEOUT)
            # Timer expired — auto fold
            if self.engine.street not in (Street.WAITING, Street.SHOWDOWN):
                valid = [a.value for a in self.engine.get_valid_actions()]
                if valid:
                    action_type = ActionType.FOLD if "fold" in valid else ActionType(valid[0])
                    state = self.engine.act(Action(type=action_type, amount=0))
                    await self.send({"type": "timeout_fold", "player": self.engine.current_player})
                    await self.send({"type": "state", "data": state})
                    if state["street"] not in ("WAITING",):
                        await self._process_ai_turns()
        except asyncio.CancelledError:
            pass

    async def start_hand(self) -> dict:
        """Start a new hand and return state."""
        self.action_log = []
        state = self.engine.start_hand()
        self._append_street()  # PREFLOP
        self._inject_action_log(state)
        await self.send({"type": "state", "data": state})
        await self._broadcast_action_log()
        await self._process_ai_turns()
        # Start timer for human turn
        if self.engine.street not in (Street.WAITING, Street.SHOWDOWN):
            self._start_turn_timer()
            await self.send({"type": "turn_timer", "deadline": self._turn_deadline, "seconds": TURN_TIMEOUT})
        return state

    async def handle_action(self, action_data: dict) -> dict:
        """Handle a human player's action."""
        self._cancel_turn_timer()
        action_type = ActionType(action_data["action"])
        amount = action_data.get("amount", 0)
        action = Action(type=action_type, amount=amount)

        prev_street = self.engine.street

        try:
            state = self.engine.act(action)
        except ValueError as e:
            # Invalid action (e.g. raise below minimum) — notify client, resend current state
            await self.send({"type": "error", "message": str(e)})
            state = self.engine.get_state()
            self._inject_action_log(state)
            await self.send({"type": "state", "data": state})
            # Restart timer for retry
            if self.engine.street not in (Street.WAITING, Street.SHOWDOWN):
                self._start_turn_timer()
                await self.send({"type": "turn_timer", "deadline": self._turn_deadline, "seconds": TURN_TIMEOUT})
            return state

        # Log human action
        player = self.engine.players[0]
        self._append_action(0, player.name, action_data["action"], amount)
        # Detect street transition
        if self.engine.street != prev_street and self.engine.street not in (Street.WAITING, Street.SHOWDOWN):
            self._append_street()
        await self._broadcast_action_log()

        self._inject_action_log(state)
        await self.send({"type": "state", "data": state})

        if state["street"] not in ("WAITING",):
            await self._process_ai_turns()

        # Start timer for next human turn
        if self.engine.street not in (Street.WAITING, Street.SHOWDOWN):
            if self.engine.players[self.engine.current_player].is_human:
                self._start_turn_timer()
                await self.send({"type": "turn_timer", "deadline": self._turn_deadline, "seconds": TURN_TIMEOUT})
        return state

    async def _process_ai_turns(self) -> None:
        """Process AI actions until it's the human's turn or hand ends."""
        while (
            self.engine.street not in (Street.WAITING, Street.SHOWDOWN)
            and self.engine.current_player < len(self.engine.players)
            and not self.engine.players[self.engine.current_player].is_human
        ):
            ai_idx = self.engine.current_player
            player = self.engine.players[ai_idx]
            if ai_idx < 1 or ai_idx - 1 >= len(self.ai_agents):
                break
            agent = self.ai_agents[ai_idx - 1]

            # Stage 1: Analyzing (visible for 0.5s)
            await self.send({"type": "ai_thinking", "player": ai_idx, "stage": "analyzing"})
            await asyncio.sleep(0.5)

            ctx = GameContext(
                hole_cards=player.hole_cards,
                board=self.engine.board,
                pot=sum(p.bet_this_hand for p in self.engine.players),
                current_bet=self.engine.current_bet,
                my_bet_this_street=player.bet_this_street,
                my_chips=player.chips,
                num_players_in_hand=self.engine.num_in_hand,
                street=self.engine.street.name,
                position=self._get_position(ai_idx),
                action_history=self._normalized_action_history(),
            )
            valid = [a.value for a in self.engine.get_valid_actions()]
            if not valid:
                await self.send({"type": "ai_thinking", "player": ai_idx, "stage": None})
                break

            # Stage 2: Computing (visible for 0.5-0.8s depending on difficulty)
            computing_label = "solving subgame" if agent.difficulty == Difficulty.ADVANCED else "computing"
            await self.send({"type": "ai_thinking", "player": ai_idx, "stage": computing_label})
            compute_delay = 0.8 if agent.difficulty == Difficulty.ADVANCED else 0.5
            await asyncio.sleep(compute_delay)

            try:
                decision = await asyncio.wait_for(
                    asyncio.get_event_loop().run_in_executor(
                        None, agent.decide, ctx, valid
                    ),
                    timeout=10.0,
                )
            except asyncio.TimeoutError:
                print(f"AI {ai_idx} timed out, folding")
                decision = {"action": "fold" if "fold" in valid else valid[0], "amount": 0}
            except Exception as e:
                print(f"AI decide error: {e}")
                decision = {"action": valid[0], "amount": 0}

            # Show "deciding" stage briefly before clearing
            await self.send({"type": "ai_thinking", "player": ai_idx, "stage": "deciding"})
            await asyncio.sleep(0.4)

            # Clear thinking status
            await self.send({"type": "ai_thinking", "player": ai_idx, "stage": None})

            chosen = decision["action"]

            # Ensure AI's chosen action is valid; fallback to first valid
            if chosen not in valid:
                chosen = valid[0]

            action_type = ActionType(chosen)
            amount = decision.get("amount", 0)

            # Fix raise without amount — default to min raise
            if action_type == ActionType.RAISE and amount <= 0:
                amount = self.engine.current_bet * 2 + self.engine.min_raise

            prev_street = self.engine.street

            try:
                state = self.engine.act(Action(type=action_type, amount=amount))
            except (ValueError, Exception) as e:
                # Fallback: try call, then fold
                print(f"AI action error ({chosen}, amt={amount}): {e}")
                fallback = None
                for fb in ["call", "check", "fold"]:
                    if fb in valid:
                        try:
                            state = self.engine.act(Action(type=ActionType(fb), amount=0))
                            fallback = fb
                            break
                        except Exception:
                            continue
                if fallback is None:
                    break

            # Log AI action
            self._append_action(ai_idx, player.name, decision["action"], decision.get("amount", 0))
            # Detect street transition
            if self.engine.street != prev_street and self.engine.street not in (Street.WAITING, Street.SHOWDOWN):
                self._append_street()
            await self._broadcast_action_log()

            await self.send({
                "type": "ai_action",
                "player": ai_idx,
                "action": decision["action"],
                "amount": decision.get("amount", 0),
            })
            self._inject_action_log(state)
            await self.send({"type": "state", "data": state})

            if state["street"] == "WAITING":
                break

    def _get_position(self, player_idx: int) -> str:
        """Determine player's position relative to the button."""
        n = self.engine.num_players
        btn = self.engine.button
        relative = (player_idx - btn) % n
        if relative == 0:
            return "late"  # button
        elif relative <= n // 3:
            return "blinds"
        elif relative <= 2 * n // 3:
            return "early"
        return "middle"

    def _normalized_action_history(self) -> list[str]:
        """Extract action sequence from engine action_log, excluding blinds.

        The C++ game tree is built with button=0, so the action order
        is already correct — we just need the action types in order.
        The tree internally tracks whose turn it is at each node,
        so the sequence of action types is sufficient.
        """
        history = []
        for entry in self.engine.action_log:
            action = entry.get("action", "")
            if action == "blind":
                continue  # blinds are baked into the tree structure
            # Map game actions to C++ abstract actions
            if action == "fold":
                history.append("fold")
            elif action == "check":
                history.append("check")
            elif action == "call":
                history.append("call")
            elif action == "raise":
                # Map raise amount to abstract sizing
                amount = entry.get("amount", 0)
                pot = sum(p.bet_this_hand for p in self.engine.players)
                if pot > 0:
                    ratio = amount / pot
                    if ratio <= 0.5:
                        history.append("raise_33")
                    elif ratio <= 1.1:
                        history.append("raise_75")
                    else:
                        history.append("raise_150")
                else:
                    history.append("raise_75")  # default to pot-sized
            elif action == "all_in":
                history.append("all_in")
        return history


# Active game rooms (simple in-memory store)
_rooms: dict[str, GameRoom] = {}


@router.websocket("/ws/game")
async def game_ws(ws: WebSocket) -> None:
    """Main WebSocket endpoint for poker game."""
    await ws.accept()

    room: GameRoom | None = None

    try:
        while True:
            raw = await ws.receive_text()
            msg = json.loads(raw)
            msg_type = msg.get("type", "")

            if msg_type == "create_game":
                # Create a new game room
                num_players = msg.get("num_players", 2)
                difficulties = msg.get("ai_difficulties", None)
                small_blind = msg.get("small_blind", 1)
                big_blind = msg.get("big_blind", 2)
                starting_chips = msg.get("starting_chips", 400)

                room = GameRoom(
                    num_players=num_players,
                    small_blind=small_blind,
                    big_blind=big_blind,
                    starting_chips=starting_chips,
                    ai_difficulties=difficulties,
                )
                room.ws = ws
                await ws.send_json({
                    "type": "game_created",
                    "data": {
                        "num_players": num_players,
                        "small_blind": small_blind,
                        "big_blind": big_blind,
                        "starting_chips": starting_chips,
                    },
                })

            elif msg_type == "start_hand":
                if room:
                    await room.start_hand()

            elif msg_type == "action":
                if room:
                    await room.handle_action(msg)

            elif msg_type == "run_it_multiple":
                if room and room.engine.all_in_showdown:
                    times = msg.get("times", 1)
                    results = room.engine.run_it_multiple(times)
                    await ws.send_json({
                        "type": "run_it_results",
                        "data": results,
                        "times": times,
                    })

            elif msg_type == "ping":
                await ws.send_json({"type": "pong"})

    except WebSocketDisconnect:
        pass
    except Exception as e:
        try:
            await ws.send_json({"type": "error", "message": str(e)})
        except Exception:
            pass
