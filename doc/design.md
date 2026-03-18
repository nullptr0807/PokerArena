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
- BB/100（每 100 手盈亏 BB 数，颜色编码：绿=盈利，红=亏损）
- 手数（已完成的牌局数）
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
  - 牌桌视图（俯视，椭圆桌面，深绿 felt 多层渐变质感）
  - 玩家座位（头像、筹码 💰、手牌、当前下注 ⬆ 金色 pill）
  - 操作面板（弃牌/过牌/跟注/加注 + Pot-sizing 预设按钮 ¼ ⅓ ½ ¾ 1x 1.2x 1.5x 2x ALL IN）
  - 公共牌区域（大尺寸 lg 卡牌 + POT 金色发光显示，居中定位）
  - 数据面板（VPIP、BB/100 颜色编码、手数）
  - 游戏设置面板（AI 数量 & 难度）
  - 牌局结果弹窗（5 秒倒计时 + 暂停/跳过 + AI 分析按钮）
  - 调试面板（AI 决策事件、迭代次数、概率分布、计算耗时）

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
- **训练日志**: `--log-every` 控制输出频率，append 模式 + 每行 flush，支持 `tail -f` 实时查看
- **训练产物**: blueprint.bin、checkpoint_*.bin、training.log，存放在 `engine/runs/<run_id>/`

## AI 对局分析
- **触发**: 每手牌结束后 5 秒窗口内点击「🧠 AI 分析对局」按钮
- **实现**: `POST /api/analyze-hand` → 后端调用 `openclaw agent --session-id poker-analysis --json`
- **Prompt**: 格式化手牌历史（中文），请求从 GTO 角度逐街分析每位玩家行动
- **展示**: Markdown 渲染（表格、标题、✅/🔴 标注），毛玻璃弹窗内显示
- **超时**: 90 秒

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
│   │   └── stats.py          # 统计数据 (VPIP, BB/100)
│   ├── api/
│   │   ├── server.py         # FastAPI 主入口
│   │   ├── ws.py             # WebSocket handler
│   │   ├── stats.py          # 统计 API
│   │   └── analyze.py        # AI GTO 分析 endpoint
│   ├── ai/
│   │   ├── agent.py          # AI 决策接口（三级难度）
│   │   ├── difficulty.py     # 难度控制 & 噪声注入
│   │   └── engine_bridge.py  # C++ 引擎桥接（pybind11）
│   └── requirements.txt
├── engine/                   # C++ GTO 引擎
│   ├── src/
│   │   ├── cfr.cpp           # MCCFR 算法实现
│   │   ├── abstraction.cpp   # 信息抽象 (手牌分桶)
│   │   ├── subgame.cpp       # 实时子博弈求解
│   │   ├── game_tree.cpp     # 博弈树构建
│   │   ├── train_main.cpp    # 离线训练入口 (--log-every)
│   │   └── bindings.cpp      # pybind11 绑定
│   ├── include/
│   ├── runs/                 # 训练产物 (blueprint, checkpoint, log)
│   ├── CMakeLists.txt
│   └── train.cpp             # 训练入口（旧）
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
- [x] 统计数据 API（VPIP、BB/100、手数）
- [x] AI 对局分析 API（OpenClaw 集成）

### Phase 4: 前端
- [x] 牌桌 UI（Apple/GGPoker 风格）
- [x] 卡牌 & 筹码动画 (Framer Motion)
- [x] 操作面板（Pot-sizing 预设按钮）
- [x] 游戏设置界面
- [x] 数据统计展示（BB/100 颜色编码）
- [x] 牌局结果展示（5 秒倒计时 + 暂停/跳过）
- [x] AI 对局 GTO 分析（Markdown 渲染）
- [x] 调试面板（迭代次数、概率分布）
- [x] 中文界面
- [ ] Run It Multiple Times UI

### Phase 5: 集成 & 优化
- [x] 端到端联调
- [x] 训练日志功能（--log-every, tail -f）
- [ ] AI 难度平衡测试
- [ ] Exploitability 测量
- [ ] 6-max 蓝图 + 多人子博弈求解
- [ ] 更精细的手牌/下注抽象优化
- [ ] 性能优化
- [ ] Docker 部署
