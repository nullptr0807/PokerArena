# 🃏 PokerArena

多人德州扑克对战平台，集成基于 MCCFR（蒙特卡洛反事实遗憾最小化）的 AI 对手。

## 项目结构

```
PokerArena/
├── engine/          # C++20 核心求解器
│   ├── include/     # 头文件（CFR、博弈树、手牌抽象、子博弈求解）
│   ├── src/         # 实现（MCCFR 训练、策略导出、手牌评估）
│   └── CMakeLists.txt
├── backend/         # Python 后端（FastAPI + WebSocket）
│   ├── api/         # HTTP/WebSocket 服务端
│   ├── game/        # 游戏引擎（发牌、下注、底池、手牌评估、统计）
│   ├── ai/          # AI 代理（C++ 引擎桥接、难度分级）
│   └── tests/       # 测试
├── frontend/        # React + TypeScript 前端
│   └── src/
│       ├── components/  # 牌桌、座位、动作面板、计时器、统计等
│       └── hooks/       # WebSocket 游戏通信
├── start.sh         # 一键启动脚本
└── Dockerfile
```

## 技术栈

| 层级 | 技术 |
|------|------|
| 求解器 | C++20, CMake, External Sampling MCCFR |
| 后端 | Python, FastAPI, Uvicorn, WebSocket |
| 前端 | React 19, TypeScript, Vite, Framer Motion |
| 容器 | Docker |

## 核心特性

- **6人桌德州扑克** — 完整游戏引擎（发牌、下注轮次、底池管理、摊牌）
- **MCCFR AI 对手** — C++ 实现的 External Sampling MCCFR 求解器
  - 手牌抽象（bucket 聚类）
  - 博弈树构建与遍历
  - 蓝图策略训练与 checkpoint 导出
  - 子博弈实时求解（subgame solving）
- **难度分级** — 不同级别 AI 对手
- **实时对战** — WebSocket 实时通信
- **游戏统计** — 胜率、VPIP 等数据追踪

## 快速开始

```bash
# 一键启动
./start.sh

# 或分别启动：

# 后端
cd backend && python -m venv venv && source venv/bin/activate
pip install -r requirements.txt
uvicorn api.server:app --host 0.0.0.0 --port 8000

# 前端
cd frontend && npm install && npx vite --port 3000

# 编译 C++ 求解器
cd engine && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
```

## 训练 AI

```bash
cd engine/build
./train              # 运行 MCCFR 训练，生成蓝图策略
```

训练产物（`blueprint.bin`、`checkpoint_*.bin`）存放在 `engine/build/data/`。

## 当前状态

- ✅ 完整游戏引擎（发牌、下注、底池、评估）
- ✅ External Sampling MCCFR 求解器
- ✅ 手牌抽象 + 博弈树
- ✅ 子博弈求解
- ✅ 蓝图策略训练 + checkpoint
- ✅ WebSocket 实时对战
- ✅ React 前端（牌桌、座位、动作面板）
- 🔲 MCCFR+ 优化（CFR+ regret 裁剪、线性/折扣加权）
- 🔲 更精细的手牌抽象
- 🔲 多桌支持

## License

Private
