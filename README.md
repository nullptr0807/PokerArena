# 🃏 PokerArena

多人德州扑克对战平台，集成基于 MCCFR（蒙特卡洛反事实遗憾最小化）的 GTO AI 对手。

## 截图

> _TODO: 添加截图_

## 项目结构

```
PokerArena/
├── engine/          # C++20 核心求解器
│   ├── include/     # 头文件（CFR、博弈树、手牌抽象、子博弈求解）
│   ├── src/         # 实现（MCCFR 训练、策略导出、手牌评估）
│   ├── runs/        # 训练产物（blueprint、checkpoint、日志）
│   └── CMakeLists.txt
├── backend/         # Python 后端（FastAPI + WebSocket）
│   ├── api/         # HTTP/WS 服务端 + AI 分析 endpoint
│   ├── game/        # 游戏引擎（发牌、下注、底池、手牌评估、统计）
│   ├── ai/          # AI 代理（C++ 引擎桥接、难度分级）
│   └── tests/       # 测试
├── frontend/        # React + TypeScript 前端
│   └── src/
│       ├── components/  # 牌桌、座位、动作面板、统计、分析等
│       └── hooks/       # WebSocket 游戏通信
├── start.sh         # 一键启动脚本
└── Dockerfile
```

## 技术栈

| 层级 | 技术 |
|------|------|
| 求解器 | C++20, CMake, External Sampling MCCFR |
| 后端 | Python 3, FastAPI, Uvicorn, WebSocket |
| 前端 | React 19, TypeScript, Vite, Framer Motion |
| 容器 | Docker |

## 核心特性

### 游戏引擎
- **完整 NL Hold'em** — 2-6人桌，发牌、下注轮次、底池（含 side pot）、摊牌
- **密码学安全发牌** — Fisher-Yates 洗牌 + `secrets` 随机数
- **Run It Multiple Times** — All-in 后支持多次发牌

### AI 对手
- **三级难度**：
  - 🟢 **普通** — Blueprint + 大量噪声，模拟常见 leak（limp 过多、fold to 3bet 过高）
  - 🟡 **中等** — Blueprint + 少量探索，接近 GTO
  - 🔴 **高级** — Blueprint + 实时子博弈求解，逐街递增迭代（翻前 2K / 翻牌 5K / 转牌 10K / 河牌 15K）
- **C++ MCCFR 引擎** — External Sampling，支持 CFR+/Linear/DCFR 变体
- **手牌抽象** — EHS² bucket 聚类
- **蓝图策略** — 已完成 500K 迭代训练（~3 GB blueprint）
- **多人桌适配** — 3+ 人时自动切换 multiplayer 策略（基于 EHS 阈值 + 对手数调整）

### 前端 UI
- **专业牌桌视觉** — Apple 简洁感 + PokerStars/GGPoker 风格，深绿 felt 质感
- **精致卡牌** — 大尺寸、serif 字体、红黑花色区分
- **Pot-sizing 预设** — ¼ ⅓ ½ ¾ 1x 1.2x 1.5x 2x ALL IN 快捷按钮
- **实时统计** — VPIP、BB/100 盈亏率（颜色编码）、手数
- **5 秒结果展示** — 毛玻璃弹窗 + 倒计时 + 暂停/跳过
- **中文界面** — 弃牌/过牌/跟注/加注/全下

### AI 对局分析
- **一键 GTO 分析** — 牌局结束后点击「🧠 AI 分析对局」
- **逐街分析** — 对每位玩家的每个行动进行 GTO 评价
- **Markdown 渲染** — 表格、标题、颜色标注（✅ 合理 / 🔴 Leak）
- **基于 OpenClaw** — 后端调用 `openclaw agent` 完成分析

### 调试面板
- **实时事件流** — AI 决策、求解迭代次数、动作概率分布
- **计算耗时** — 每次决策的 compute_ms / total_ms
- **手牌日志** — JSON 格式完整手牌记录

## 快速开始

```bash
# 一键启动（后端 :8000 + 前端 :3000）
./start.sh

# 或分别启动：

# 后端
cd backend && source venv/bin/activate
uvicorn api.server:app --host 0.0.0.0 --port 8000 --reload

# 前端
cd frontend && npm install && npx vite --port 3000

# 编译 C++ 求解器
cd engine && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
```

## 训练 AI

```bash
cd engine/build
./train --iterations 500000 --log-every 10000

# 训练产物保存在 runs/ 目录，包含：
# - blueprint.bin（蓝图策略）
# - checkpoint_*.bin（中间 checkpoint）
# - training.log（训练日志，支持 tail -f 实时查看）
```

## 当前状态

- ✅ 完整游戏引擎（发牌、下注、底池、评估、side pot）
- ✅ External Sampling MCCFR 求解器（C++20）
- ✅ 手牌抽象（EHS² bucket 聚类）+ 博弈树
- ✅ 子博弈实时求解（逐街递增迭代）
- ✅ 蓝图策略训练 + checkpoint + 训练日志
- ✅ 三级 AI 难度（普通/中等/高级）
- ✅ WebSocket 实时对战
- ✅ 专业级前端 UI（牌桌、卡牌、动画、统计）
- ✅ Pot-sizing 预设按钮
- ✅ AI 对局 GTO 分析（OpenClaw 集成）
- ✅ 暂停/继续、5 秒结果展示
- ✅ 调试面板（迭代次数、概率分布）
- ✅ BB/100 标准盈亏指标
- 🔲 更精细的手牌抽象优化
- 🔲 6-max 蓝图 + 多人子博弈求解
- 🔲 AI 难度平衡测试 & exploitability 测量
- 🔲 Docker 部署

## License

Private
