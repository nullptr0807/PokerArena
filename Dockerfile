# Multi-stage build for PokerArena

# Stage 1: Build C++ engine
FROM python:3.14-slim AS cpp-builder
RUN apt-get update && apt-get install -y cmake g++ pybind11-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app/backend/ai/cpp
COPY backend/ai/cpp/ .
RUN mkdir build && cd build && cmake .. -Wno-dev && make -j$(nproc)

# Stage 2: Build frontend
FROM node:22-slim AS frontend-builder
WORKDIR /app/frontend
COPY frontend/package.json frontend/package-lock.json* ./
RUN npm install
COPY frontend/ .
RUN npm run build

# Stage 3: Production
FROM python:3.14-slim
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Backend
COPY backend/requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt
COPY backend/ ./backend/

# C++ engine
COPY --from=cpp-builder /app/backend/ai/cpp/build/*.so backend/ai/cpp/build/
COPY --from=cpp-builder /app/backend/ai/cpp/build/train_blueprint backend/ai/cpp/build/

# Frontend static files
COPY --from=frontend-builder /app/frontend/dist ./frontend/dist

EXPOSE 8000

WORKDIR /app/backend
CMD ["python", "-m", "uvicorn", "api.server:app", "--host", "0.0.0.0", "--port", "8000"]
