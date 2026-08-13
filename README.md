# 冲量荒原（暂定名）· 灰盒原型工程

> 3D 俯视角物理动作游戏 · UE 5.8 · 独立原型工程（工作区 `C:\workspace\Vector`）
>
> **一句话概念**：玩家在程序生成的异星荒原中猎杀生物、收集器官并制造动能装备；战斗的关键不是持续砍血，而是破坏敌人的稳定性、改变其质量与运动状态，再把怪物撞向怪物，以连锁碰撞完成狩猎。
> **核心宣传句**：怪物不是沙包，是你的弹药。

---

## 当前状态（2026-08-13）

**灰盒原型核心闭环已达成，用户实测确认**：

- ✅ 冲量锤：蓄力 → 动量推出（轻 12m / 中 4m / 重 1.2m，动量定理 Δv=I/m）
- ✅ 稳定/失衡两层模型：稳定度（60，归零失衡倒地）/ 生命（100，归零死亡）
- ✅ 失衡脱锚：重型失衡后有效质量 5.0→2.0，满蓄 875 cm/s 撞墙 50 伤害（2 次击杀）
- ✅ 碰撞连锁：撞人扣血+失衡+动量传递 40%，撞墙反噬，落地震荡 AOE
- ✅ 敌人三型：跳囊虫（轻群）/ 甲壳犀（重慢）/ 角槌兽（冲锋预警 0.5s→1600 冲量），NavMesh 寻路绕墙
- ✅ 保底伤害：锤击 15 生命/锤，7 锤磨死（物理非唯一解）
- ✅ 跳跃（躲冲锋）+ 滚轮缩放 + 饥荒式固定俯角旋转
- ✅ 性能：11 只同屏 60 帧（孤儿 AIController bug 已修）
- ⏳ 10 条验收 Gate 正式裁决中（见 `Docs/待办清单.md`）

## 目录结构

```
Vector/
├── Vector/                     # UE 5.8 工程（模块名 Vector）
│   ├── Source/Vector/          # C++ 源码
│   │   ├── Public/Gameplay/    # 受控冲量移动组件（统一施力入口）
│   │   ├── Public/Combat/      # 冲量锤/碰撞连锁/生命/敌人三型/动作状态机
│   │   ├── Public/Stability/   # 稳定/失衡账本（纯 C++ 可测）
│   │   ├── Public/Impact/      # 碰撞伤害数学（纯函数）
│   │   └── Private/Tests/      # Automation 测试（12 项）
│   ├── Content/Prototype/      # 灰盒地图 + SK_Robot 机器人占位
│   └── Config/
├── Design/原型设计基线_v0.1.md # 轻量设计基线（三项门槛/10 条验收/布局）
├── Docs/可复用资产与移植清单.md # 移植记录 + 数值账本 + 各 S 落地记录
├── Docs/待办清单.md            # P0-P3 待办
└── Tools/Greybox/              # 灰盒场景一键生成脚本（Editor Python）
```

## 快速开始

1. **编译**：VS 打开 `Vector/Vector.slnx`，Development Editor / Win64，`Ctrl+Shift+B`；新增类/UPROPERTY 变更后**重启编辑器**
2. **生成灰盒竞技场**（可选）：编辑器 `Tools → Execute Python Script` 跑 `Tools/Greybox/setup_greybox_arena.py` → `Build → Build Paths` → Play
3. **手动试玩**：新关卡放 PlayerStart + 设 World Settings GameMode Override=`VectorGameMode`；Place Actors 搜 `VectorTestDummy`/`VectorEnemy` 放置
4. **测试**：编辑器 Session Frontend → Automation，搜 `Vector.` 前缀跑全部（本机 Zen 异常，勿用命令行无头跑）

## 数值账本（当前）

| 项 | 值 |
|---|---|
| 稳定度 / 生命 | 60 / 100 |
| 锤击 | 稳定度 30×蓄力 + 生命 15×蓄力 |
| 碰撞伤害 | (速度−300)×0.05 × 质量(0.8/1.0/1.4) × 类型(撞人1.0/墙1.5/落地1.5)，上限 50 |
| 推速 | Δv = 1750×蓄力 ÷ 质量（失衡脱锚质量 轻1.0/中1.5/重2.0） |
| 冲锋 | 预警 0.5s → 1600 cm/s → 冷却 4s |
| 镜头 | 固定 55° 俯角，鼠标水平旋转，滚轮 600-2400cm |

## 设计约束（红线）

- 唯一动作阶段枚举 `EVectorActionPhase`（Idle→Windup→Active→Recovery），互斥走共享仲裁（不复制 Morphorbit 的 4 套 Phase）
- 平面世界：局部朝上恒 = +Z，禁止移植径向/球面抽象
- 只有"冲量驱动的物理运动"才结算碰撞伤害（站桩互挤不伤友军）
- 物理击杀比磨血快约 2-3 倍，但不能成为唯一解（保底伤害始终有效）

## 技术备忘

- 本机 UE 命令行无头跑 Automation 会卡 ZenServer → 测试一律编辑器 GUI
- `bRunPhysicsWithNoController` 对无 Controller 靶子必须开 true，否则冲量永不消费
- AIController 不随 Pawn 销毁 → 敌人死亡必须显式销毁控制器（否则孤儿控制器堆积掉帧）
