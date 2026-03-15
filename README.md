# 🃏 Poker Arena

No-Limit Texas Hold'em Cash Game — Human vs AI

## Quick Start

```bash
./start.sh
```

Then open http://localhost:3000

## Manual Start

```bash
# Backend
cd backend && source venv/bin/activate
uvicorn api.server:app --host 0.0.0.0 --port 8000

# Frontend (in another terminal)
cd frontend && npm run dev
```

## Features

- **2-6 players** (1 human + 1-5 AI)
- **3 AI difficulty levels**: 普通 / 中等 / 高级
- **Real-time WebSocket** game state sync
- **Framer Motion** card animations
- **Run It Multiple Times** (1-3x on all-in)
- **Player stats**: VPIP, BB/hand tracking
- **Cryptographically secure** card shuffle (Fisher-Yates + `secrets`)

## Architecture

```
backend/
  game/       Core engine (deck, evaluator, pot, stats)
  ai/         AI agent with difficulty-based strategy
  api/        FastAPI + WebSocket server
  tests/      24 unit tests
frontend/
  src/
    components/   Card, Table, PlayerSeat, ActionPanel, Setup, RunItMultiple
    hooks/        useGameSocket (WebSocket client)
    styles/       Global CSS (dark theme, Apple-inspired)
```

## Tech Stack

- **Backend**: Python 3.14, FastAPI, WebSocket, treys (hand evaluation)
- **Frontend**: React 19, TypeScript, Vite, Framer Motion
- **AI**: Heuristic-based (future: C++ MCCFR via pybind11)

## Game Rules

- Fixed blinds (default 1/2)
- 200BB starting stacks
- Standard NLHE rules
