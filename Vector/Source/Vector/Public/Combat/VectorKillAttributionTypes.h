// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorKillAttributionTypes.generated.h"

/**
 * 击杀归因来源（P0：验收 #6 硬证据 + 死亡检查点监控）。
 *
 * 命名用 Cause 而非 Phase，避免与原项目"第 N 套 Phase 枚举"的 DEBT-01 混淆。
 * 每个致死瞬间由伤害结算方上报一次，GameMode 上的归因组件计数；
 * 局末按占比检测"单一来源 > 阈值"（默认 60%）并报警，防推墙唯一解退化。
 */
UENUM(BlueprintType)
enum class EVectorKillCause : uint8
{
	/** 冲量锤直接锤死（保底路径，物理非唯一解的证据）。 */
	Hammer,

	/** 被推/冲锋撞墙自伤致死（推墙流）。 */
	WallCollision,

	/** 撞到其他敌人致死（含动量传递连锁）。 */
	BodyCollision,

	/** 冲锋中的角槌兽撞死其他敌人（免费炮弹/借力打力）。 */
	ChargerRam,

	/** 落地震荡 AOE 致死（挑飞→下砸流）。 */
	LandingShock,

	/** 其他/未知来源。 */
	Other,

	/** 哨兵：枚举数量，不入蓝图。 */
	Count UMETA(Hidden)
};
