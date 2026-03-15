#!/bin/bash
# PokerArena — start both backend and frontend
set -e

cd "$(dirname "$0")"

echo "🃏 Starting PokerArena..."

# Backend
echo "  → Starting backend (port 8000)..."
cd backend
source venv/bin/activate
python -m uvicorn api.server:app --host 0.0.0.0 --port 8000 &
BACKEND_PID=$!
cd ..

# Wait for backend
sleep 2

# Frontend
echo "  → Starting frontend (port 3000)..."
cd frontend
npx vite --port 3000 &
FRONTEND_PID=$!
cd ..

echo ""
echo "✅ PokerArena is running!"
echo "   Frontend: http://localhost:3000"
echo "   Backend:  http://localhost:8000"
echo "   Health:   http://localhost:8000/health"
echo ""
echo "Press Ctrl+C to stop."

trap "kill $BACKEND_PID $FRONTEND_PID 2>/dev/null; exit" INT TERM
wait
