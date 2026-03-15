# Poker Arena Design Doc

## 目标
你的目标是，实现这个文档中计划，同时你可以修改这个文档，但注意不要修改“目标部分”
遇到问题尽量自己处理

## 概述
网页版德州扑克对战平台，用户 vs AI 虚拟对手，支持 2-6 人桌。

## 游戏规则
- **模式**: No-Limit Texas Hold'em Cash Game
- **盲注**: 固定盲注，无前注 (e.g. 1/2)
- **人数**: 2-6 人（1 个真人 + 1-5 个 AI）
- **起始筹码**: 每个 session 每人 200BB
- **Run It Multiple Times**: All-in 后支持发 1-3 次，由用户选择
- **发牌**: 密码学安全随机数 + Fisher-Yates 洗牌

## AI 难度等级
游戏开始前可设置每个 AI 对手的难度：
| 等级 | 描述 | 实现策略 |
|------|------|----------|
| 普通 | 普通玩家水平 | Blueprint 策略 + 大量随机噪声，模拟常见 leak（limp 过多、fold to 3bet 过高等） |
| 中等 | 非常强的人类玩家 | Blueprint 策略 + 少量探索，接近但不完全 GTO |
| 高级 | 尽可能接近 GTO 最优 | Blueprint + 实时子博弈求解 (Real-time subgame solving) |

## 数据统计
每个玩家实时显示：
- VPIP (Voluntarily Put $ In Pot)
- 平均每手盈亏 (bb/hand)
- 当前筹码量

## 技术架构

### 整体架构
```
┌─────────────┐     WebSocket      ┌──────────────┐      FFI/subprocess      ┌─────────────┐
│   Frontend   │ ◄──────────────► │  Python API   │ ◄──────────────────────► │  C++ Engine  │
│  (React/TS)  │                   │  (FastAPI)    │                          │  (CFR/GTO)   │
└─────────────┘                   └──────────────┘                          └─────────────┘
```

### 前端 (TypeScript + React)
- **框架**: React + TypeScript + Vite
- **动画**: Framer Motion（流畅的卡牌/筹码动画）
- **设计风格**: Apple/OpenAI 风格 — 简洁、大量留白、精致的阴影和过渡
- **通信**: WebSocket（实时游戏状态同步）
- **UI 组件**:
  - 牌桌视图（俯视，椭圆桌面）
  - 玩家座位（头像、筹码、手牌）
  - 操作面板（Fold / Check / Call / Raise slider）
  - 公共牌区域（带翻牌动画）
  - 筹码池动画
  - 数据面板（VPIP、bb/hand）
  - 游戏设置面板（AI 数量 & 难度）

### 后端 — 游戏服务 (Python)
- **框架**: FastAPI + WebSocket
- **职责**:
  - 游戏状态管理（发牌、回合流转、底池计算）
  - 玩家行动验证
  - 多次发牌 (Run It Multiple Times) 逻辑
  - 统计数据计算
  - 调用 C++ 引擎获取 AI 决策

### 后端 — AI 引擎 (C++)
- **算法**: Monte Carlo CFR (MCCFR) — 适合多人不完全信息博弈
- **信息抽象**:
  - 手牌分桶: 基于 EHS (Effective Hand Strength) 聚类
  - 下注抽象: 离散化下注尺寸 (e.g. 0.33pot, 0.5pot, 0.75pot, pot, 1.5pot, all-in)
- **两阶段策略**:
  1. **离线预训练**: MCCFR 训练 blueprint 策略，存储为策略表
  2. **在线求解**: 高级难度使用 depth-limited subgame solving 实时细化策略
- **难度控制**:
  - 普通: 从 blueprint 采样 + 注入噪声 (epsilon-greedy, ε=0.3) + 人为 leak
  - 中等: blueprint 直接使用，少量探索 (ε=0.05)
  - 高级: blueprint + 实时子博弈求解
- **接口**: 通过 pybind11 暴露为 Python 模块，或通过 subprocess + JSON 通信
- **性能目标**: 每次决策 < 30 秒（高级难度含子博弈求解）

## 项目结构
```
PokerArena/
├── doc/
│   └── design.md
├── frontend/                 # React 前端
│   ├── src/
│   │   ├── components/       # UI 组件
│   │   ├── hooks/            # 游戏状态 hooks
│   │   ├── styles/           # 全局样式
│   │   └── App.tsx
│   ├── package.json
│   └── vite.config.ts
├── backend/                  # Python 游戏服务
│   ├── game/
│   │   ├── engine.py         # 游戏状态机
│   │   ├── deck.py           # 发牌 & 洗牌
│   │   ├── evaluator.py      # 牌力评估
│   │   ├── pot.py            # 底池计算 (含 side pot)
│   │   └── stats.py          # 统计数据
│   ├── api/
│   │   ├── server.py         # FastAPI 主入口
│   │   └── ws.py             # WebSocket handler
│   ├── ai/
│   │   ├── agent.py          # AI 决策接口
│   │   └── difficulty.py     # 难度控制 & 噪声注入
│   └── requirements.txt
├── engine/                   # C++ GTO 引擎
│   ├── src/
│   │   ├── cfr.cpp           # MCCFR 算法实现
│   │   ├── abstraction.cpp   # 信息抽象 (手牌分桶)
│   │   ├── subgame.cpp       # 实时子博弈求解
│   │   ├── game_tree.cpp     # 博弈树构建
│   │   └── bindings.cpp      # pybind11 绑定
│   ├── include/
│   ├── data/                 # 预训练策略表
│   ├── CMakeLists.txt
│   └── train.cpp             # 离线训练入口
└── README.md
```

## 开发计划

### Phase 1: 核心游戏引擎 (Python)
- [x] 德扑游戏状态机（发牌、下注轮、摊牌）
- [x] 牌力评估器
- [x] 底池计算（含 side pot）
- [x] 单元测试

### Phase 2: C++ AI 引擎
- [x] Python AI Agent（临时替代，3 难度等级）
- [x] 手牌抽象 & EHS 计算
- [x] 博弈树构建
- [x] MCCFR 训练器
- [x] Blueprint 策略预训练
- [x] 子博弈求解器
- [x] pybind11 接口
- [x] 难度等级实现（C++ 版）
- [x] Python ↔ C++ 桥接层

### Phase 3: 后端 API
- [x] FastAPI + WebSocket 服务
- [x] 游戏房间管理
- [x] AI 集成
- [ ] 统计数据 API

### Phase 4: 前端
- [x] 牌桌 UI
- [x] 卡牌 & 筹码动画 (Framer Motion)
- [x] 操作面板
- [x] 游戏设置界面
- [x] 数据统计展示
- [ ] Run It Multiple Times UI

### Phase 5: 集成 & 优化
- [ ] 端到端联调
- [ ] AI 难度平衡测试
- [ ] 性能优化
- [ ] 部署脚本
