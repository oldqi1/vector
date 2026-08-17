# 冲量荒原（暂定名）

> UE 5.8 · PC 键鼠优先 · 俯视/斜俯视 3D 系统型物理动作狩猎 + 轻 Roguelite
> **怪物不是沙包，是会反抗的弹药；场地不是背景，是一台可操作的机器。**

玩家读取敌人和场地制造的运动威胁，用矢量枪、绳线、物性工具与高度转换改变速度、约束和落点，再把实体弹与怪物送回敌群和大型猎物。

## 当前里程碑

当前执行硬预算 30 小时，目标不是冒充内容/美术完整的公开 Demo，而是尽可能接近对外试玩标准的**可分发定向机制验证包**：

- 能独立冷启动；
- 有约 12–20 分钟完整普通房—成长—Boss—撤离路线；
- 风格统一、核心预警与物理结果可读；
- 保留 PCG × 3D × 统一物理基础框架；
- 第 30 小时结束前提供实际打包文件，而不只是源码或 Editor 地图。

当前唯一逐小时排期：[Demo30小时逐小时工作表](Docs/Demo30小时逐小时工作表_2026-08-17.md)。

## 文档

从 [Docs/README.md](Docs/README.md) 进入。当前必读只有五份：

- [Demo 系统设计 v0.6](Docs/冲量荒原_Demo系统设计_v0.6.md)
- [PCG × 3D × 物理玩法基础框架](Docs/PCG_3D物理玩法基础框架.md)
- [3D 垂直换向与高度电路规格](Docs/3D核心化v3_数学修正与压缩路线_2026-08-17.md)
- [30 小时逐小时工作表](Docs/Demo30小时逐小时工作表_2026-08-17.md)
- [当前状态与待办](Docs/待办清单.md)

## 当前代码状态

已有运行证据：

- 矢量枪、质量响应、结构伤害衰减和实体动能球改向；
- 双端绳线、润滑/浮化、升空叉、撞墙爆发与落地震荡；
- 玩家/敌人生命、死亡重生、虚空回收；
- 器官、遭遇账本、清房开门、三选一成长和撤离；
- 确定性 PCG 模块、房间触发、波次门、Boss 波次与 Nav 安全处理；
- 上一轮 Editor Automation：46 Success / 0 Failed。

已编码、等待当前完整编译/PIE：

- 电弧壳机的有限弱追踪蓝球；
- 腐蚀无人机的单发/三向绿色实体弹；
- 矢量枪解除蓝球导引；
- Boss Resolve 抗无限僵直与循环动能弹补给；
- 最新 PCG 接缝、寻路和敌群功能槽调整。

高度换向、真实落点预测以及“按住—拖动—释放”的定向砸落已进入运行时验证；环境换向器仍处于设计规格，尚未编码。

## 快速开始

1. 使用 Visual Studio 打开 `Vector/Vector.slnx`，目标 `VectorEditor / Win64 / Development`。
2. 新增反射类或属性后，关闭 Unreal Editor 或关闭 Live Coding 再完整编译。
3. 在 Editor 中执行 `Tools/Greybox/setup_tactical_pcg_preview.py` 生成当前 PCG 灰盒。
4. 执行 `Build → Build Paths`。
5. PIE 运行完整路线。
6. Session Frontend → Automation，搜索 `Vector.` 运行全部测试。

提交前离线检查：

```powershell
powershell -ExecutionPolicy Bypass -File Tools/verify_prototype_offline.ps1
```

本机命令行无头 Automation 会卡 ZenServer，Automation 使用 Editor Session Frontend。

## 当前操作

- `WASD`：移动
- `Space`：跳跃
- `1`：矢量枪
- `2`：双端绳线枪
- `3`：润滑剂
- `4`：浮化孢子
- `5`：升空叉
- `LMB`：使用当前工具
- `RMB` 或 `MMB` 按住拖动：旋转镜头（当前代码状态）
- 鼠标滚轮：缩放
- `Z / X / C`：清场三选一

## 基础红线

- 同一 Seed、版本和输入必须生成相同战术布局；
- 每个战斗模块必须具备 Source、Converter、Receiver、Recovery 和两条不同首操作 Recipe；
- 删除关键装置或压平高度后，至少一条高价值路线必须消失；
- 敌人、弹体、工具和装置复用位置、速度、质量、法线、摩擦、重力、约束和结构事实；
- Boss 可以通过质量/锚定改变响应，但不能物理免疫；
- 物理路线应明显优于磨血，但不能成为唯一可完成路线；
- 日志 PASS 不等于玩家看得懂或觉得好玩。

## 目录

```text
Vector/                 Unreal 工程
  Source/Vector/        C++ 运行时与 Automation
  Content/Vector/       当前项目内容
Tools/Greybox/          灰盒与测试场景脚本
Tools/Art/              占位美术导入脚本
ArtSource/              第三方源资产与原始许可证
Docs/                   当前设计、排期、状态和必要参考
```

第三方 Quaternius Animated Monster Pack 使用 CC0 1.0；原始授权位于 `ArtSource/QuaterniusAnimatedMonsterPack/LICENSE.txt`。
