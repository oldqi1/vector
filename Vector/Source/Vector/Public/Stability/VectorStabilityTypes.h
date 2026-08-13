// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorStabilityTypes.generated.h"

/**
 * 稳定状态机的互斥状态（原型期四态）。
 *
 * 基线 §5 恢复稳定：稳定 → 失衡 → 物理 → 倒地 → 起身。
 * S01 先落地四态：Stable → Unbalanced → Downed → Rising；"物理"态（被施力飞行）
 * 由 S02 受控冲量接入，届时在 Unbalanced 前叠加（不新增本枚举，见红线 DEBT-01）。
 *
 * 命名用 State 而非 Phase：本枚举表达"稳定度生命周期"，不是装备动作阶段，
 * 避免与原项目 4 套动作 Phase 枚举混淆。
 */
UENUM(BlueprintType)
enum class EVectorStabilityState : uint8
{
	/** 稳定：可以正常行动。 */
	Stable,

	/** 失衡：稳定度刚归零的短促硬直，随后自动进入倒地。 */
	Unbalanced,

	/** 倒地：无法行动，持续 DownedDurationSeconds。 */
	Downed,

	/** 起身：恢复站立，持续 RisingDurationSeconds。 */
	Rising,
};

/** 质量三档（基线 §1 物理门槛）。S01 只提供系数接口，数值由组件 UPROPERTY 配置。 */
UENUM(BlueprintType)
enum class EVectorMassClass : uint8
{
	Light,
	Medium,
	Heavy,
};

/** 碰撞类型系数（基线公式"相对速度 × 质量系数 × 碰撞类型系数"）。 */
UENUM(BlueprintType)
enum class EVectorImpactType : uint8
{
	/** 身体互撞 / 武器命中。 */
	Body,

	/** 撞墙（高速正面撞击，对应原项目"撞脊"语义）。 */
	Wall,

	/** 落地震荡。 */
	Ground,
};

/**
 * 稳定/失衡核心账本（A1 移植自 MorphorbitBackjetBeast.cpp L21-28 / L975-1035）。
 *
 * 纯 C++ 结构，无 UObject / World 依赖，运行时与 Automation 共用同一逻辑：
 * 稳定度 60，基础失衡量 20-30（弱点乘数 1.5 叠加），归零 → 重置 60 并进入失衡，
 * 按固定时长单向推进 失衡 → 倒地 → 起身 → 稳定。
 *
 * 数值字段由宿主组件（UVectorStabilityComponent）的 UPROPERTY 同步覆盖。
 */
struct VECTOR_API FVectorStabilityLedger
{
	/** 稳定度上限，归零后重置。移植自 MaximumStability=60。 */
	double MaximumStability = 60.0;

	/** 基础失衡量下限。移植自 MinimumBaseStaggerDamage=20。 */
	double MinimumBaseStaggerDamage = 20.0;

	/** 基础失衡量上限。移植自 MaximumBaseStaggerDamage=30。 */
	double MaximumBaseStaggerDamage = 30.0;

	/** 弱点/开放窗口乘数。移植自 WeakpointStaggerMultiplier=1.5。 */
	double WeakpointStaggerMultiplier = 1.5;

	/** 失衡硬直时长，单位 s。原型期短暂值。 */
	double UnbalancedDurationSeconds = 0.20;

	/** 倒地时长，单位 s。移植自 StaggeredRecoveryDurationSeconds=1.60。 */
	double DownedDurationSeconds = 1.60;

	/** 起身时长，单位 s。原型期新增合理值。 */
	double RisingDurationSeconds = 0.40;

	EVectorStabilityState State = EVectorStabilityState::Stable;
	double Stability = 60.0;
	double StateSecondsRemaining = 0.0;

	/** 一次命中结算的只读事实。 */
	struct FHitResult
	{
		/** 是否接受本次命中（基础失衡值有限即为接受）。 */
		bool bAccepted = false;

		/** 实际施加的稳定度伤害（含弱点/质量/碰撞类型乘数）。 */
		double AppliedStabilityDamage = 0.0;

		/** 本次命中是否让稳定度穿过零并触发失衡。 */
		bool bTriggeredStagger = false;
	};

	/**
	 * 结算一次撞击失衡。
	 *
	 * @param BaseStaggerDamage      基础失衡量（通常来自 FVectorImpactMath::ComputeStaggerDamage，20-30）。
	 * @param MassMultiplier         质量系数（组件按 EVectorMassClass 查表）。
	 * @param CollisionTypeMultiplier 碰撞类型系数（组件按 EVectorImpactType 查表）。
	 * @return 本次结算事实；非有限输入返回 bAccepted=false。
	 */
	FHitResult ReceiveImpactHit(
		double BaseStaggerDamage,
		double MassMultiplier = 1.0,
		double CollisionTypeMultiplier = 1.0);

	/**
	 * 按真实帧时间推进状态机（Stable 时无操作）。
	 * @param DeltaSeconds 本帧有限正时长，单位 s；非法值安全返回。
	 */
	void AdvanceState(double DeltaSeconds);

	/** 恢复首局账本：稳定度回满、状态回 Stable、清零计时。 */
	void Reset();

	/** 是否处于任何失衡/倒地/起身恢复状态。 */
	bool IsStaggered() const { return State != EVectorStabilityState::Stable; }

private:
	/** 写入唯一新状态并启动对应计时（非负时长）。 */
	void EnterState(EVectorStabilityState NewState, double Seconds);
};
