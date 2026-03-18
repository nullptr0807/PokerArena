"""Hand analysis endpoint — sends hand history to OpenClaw for GTO analysis."""

from __future__ import annotations

import asyncio
import os
from fastapi import APIRouter

router = APIRouter()

# Ensure subprocess can find node + openclaw
_ENV = {
    **os.environ,
    "PATH": "/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:" + os.environ.get("PATH", ""),
}


def _format_hand_for_analysis(data: dict) -> str:
    """Format hand history into a readable prompt for GTO analysis."""
    players = data.get("players", [])
    actions = data.get("action_log", [])
    result = data.get("result", {})
    blinds = data.get("blinds", {"small": 1, "big": 2})

    lines = [
        f"请从 GTO 角度分析以下德州扑克牌局（NL{blinds['big'] * 100} {len(players)}人桌，盲注 {blinds['small']}/{blinds['big']}）：",
        "",
        "## 玩家",
    ]

    for p in players:
        cards = p.get("hole_cards", "未知")
        if isinstance(cards, list):
            cards = " ".join(cards)
        lines.append(f"- {p['name']}（{'人类' if p.get('is_human') else 'AI'}）: 手牌 {cards}, 起始筹码 {p.get('chips', '?')}")

    lines.append("")

    for entry in actions:
        if entry.get("type") == "street":
            street = entry["street"]
            board_cards = " ".join(entry.get("board", []))
            street_names = {"PREFLOP": "翻前", "FLOP": "翻牌", "TURN": "转牌", "RIVER": "河牌"}
            name = street_names.get(street, street)
            lines.append(f"### {name}" + (f"（公共牌: {board_cards}）" if board_cards else ""))
        else:
            pname = entry.get("name", f"Player {entry.get('player', '?')}")
            action = entry.get("action", "?")
            amount = entry.get("amount", 0)
            action_names = {"fold": "弃牌", "check": "过牌", "call": "跟注", "raise": "加注", "all_in": "全下"}
            a = action_names.get(action, action)
            if amount and action in ("raise", "call", "all_in"):
                lines.append(f"- {pname}: {a} {amount}")
            else:
                lines.append(f"- {pname}: {a}")

    lines.append("")

    if result and result.get("winners"):
        lines.append("## 结果")
        for w in result["winners"]:
            p_idx = w.get("player", 0)
            p_name = players[p_idx]["name"] if p_idx < len(players) else f"Player {p_idx}"
            hand_rank = w.get("hand_rank", "")
            lines.append(f"- {p_name} 赢得 {w.get('amount', 0)} {'(' + hand_rank + ')' if hand_rank else ''}")

    lines.append("")
    lines.append("请对每个玩家在每条街的行动进行 GTO 分析。指出哪些是最优打法，哪些是偏差（leak），并解释原因。分析应简洁专业，用中文回复。")

    return "\n".join(lines)


@router.post("/api/analyze-hand")
async def analyze_hand(data: dict) -> dict:
    """Analyze a completed hand using OpenClaw CLI."""
    prompt = _format_hand_for_analysis(data)

    # Try multiple ways to invoke openclaw
    commands = [
        ["/opt/homebrew/bin/openclaw", "agent", "--message", prompt, "--thinking", "low", "--session-id", "poker-analysis", "--json"],
    ]

    last_err = ""
    for cmd in commands:
        try:
            proc = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=_ENV,
            )
            stdout, stderr = await asyncio.wait_for(
                proc.communicate(),
                timeout=90,
            )

            if proc.returncode == 0:
                raw = stdout.decode().strip()
                try:
                    import json
                    result = json.loads(raw)
                    # Extract text from openclaw agent --json output
                    payloads = result.get("result", {}).get("payloads", [])
                    if payloads:
                        analysis = payloads[0].get("text", "")
                    else:
                        analysis = result.get("reply", result.get("message", raw))
                except (json.JSONDecodeError, TypeError, KeyError):
                    analysis = raw
                if analysis:
                    return {"analysis": analysis, "error": None}
                last_err = "空回复"
                continue

            last_err = stderr.decode()[:300]
        except asyncio.TimeoutError:
            return {"error": "分析超时（90秒）", "analysis": None}
        except FileNotFoundError:
            last_err = f"未找到 {cmd[0]}"
            continue
        except Exception as e:
            last_err = str(e)
            continue

    return {"error": f"分析失败: {last_err}", "analysis": None}
