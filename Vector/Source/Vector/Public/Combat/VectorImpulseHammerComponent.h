// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "Stability/VectorStabilityTypes.h"
#include "VectorImpulseHammerComponent.generated.h"

class UVectorCharacterMovementComponent;
class UVectorStabilityComponent;

/**
 * 冲量锤：近距离蓄力攻击，把失衡/稳定目标作为"弹药"水平推出。
 *
 * 行为（对齐设计案 v0.1 与原型基线）：
 * - 左键按住 → Windup 蓄力（ChargeProgress 0~1）；
 * - 松开 → Active：朝镜头 Yaw（瞄准方向）对前方最近的可推目标施加水平冲量，
 *   并结算一次稳定度伤害（BaseStaggerDamage × 蓄力进度）；
 * - Recovery 固定时长后回 Idle。
 *
 * 施力统一走 UVectorCharacterMovementComponent::QueueWorldVelocityChange
 * （受控冲量队列，不直接写刚体/Velocity）；动作阶段统一走 FVectorActionTimeline
 * （红线 DEBT-01：无自定义 Phase 枚举）。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorImpulseHammerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorImpulseHammerComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 左键按下：发起蓄力（仅 Idle 可进入）。 */
	void StartCharge();

	/** 左键松开：释放冲量并结算命中（仅 Windup 有效）。 */
	void ReleaseCharge();

	/** 是否正在蓄力。 */
	bool IsCharging() const { return Timeline.Phase == EVectorActionPhase::Windup; }

	/** 当前蓄力进度 0~1（表现用）。 */
	float GetChargeProgress() const { return static_cast<float>(Timeline.ChargeProgress); }

	/** 当前动作阶段（表现/诊断用）。 */
	EVectorActionPhase GetPhase() const { return Timeline.Phase; }

	// ---- 手感数值（Details 可调） ----

	/**
	 * 满蓄冲量强度（动量 I = ImpulseSpeed × Charge），目标速度变化 = I / 相对质量。
	 * 默认 1750：对相对质量 2.5 的中型目标产生 700 cm/s。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer", meta = (ClampMin = "0.0", Units = "cm/s"))
	double ImpulseSpeedCmPerSecond = 1750.0;

	/** 命中稳定度伤害（× 蓄力进度），满蓄力时对齐 30 档。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer", meta = (ClampMin = "0.0"))
	double BaseStaggerDamage = 30.0;

	/** 命中保底核心生命伤害（× 蓄力进度）——不靠物理也能磨死（物理不能成软锁）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer", meta = (ClampMin = "0.0"))
	double BaseHealthDamage = 15.0;

	/** 命中检测距离（自角色原点沿施力方向），cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer", meta = (ClampMin = "0.0", Units = "cm"))
	double HitReachCm = 320.0;

	/** 命中检测球半径，cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer", meta = (ClampMin = "0.0", Units = "cm"))
	double HitRadiusCm = 140.0;

	/** 蓄力时间线账本（四阶段共享，见 VectorActionTypes.h）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Hammer")
	FVectorActionTimeline Timeline;

	// ---- 质量档相对质量（动量模型：Δv = 冲量 ÷ 相对质量；重物难推） ----

	/** 轻量目标相对质量（默认 1.25，满蓄速度 1400）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Mass", meta = (ClampMin = "0.1"))
	double MassValueLight = 1.25;

	/** 中型目标相对质量（默认 2.5，满蓄速度 700）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Mass", meta = (ClampMin = "0.1"))
	double MassValueMedium = 2.5;

	/** 重型目标相对质量（默认 5.0，满蓄速度 350，几乎推不动）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Mass", meta = (ClampMin = "0.1"))
	double MassValueHeavy = 5.0;

	// ---- 失衡（脱锚）质量档：失衡目标有效质量大幅下降，成为可用的"炮弹" ----
	// 对齐设计案"失衡状态：可被推出/改变属性"——重型平时像山，失衡后最凶。
	// 默认 轻 1.0 / 中 1.5 / 重 2.0：重物失衡满蓄 1750/2.0 = 875 cm/s。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Mass|Staggered", meta = (ClampMin = "0.1"))
	double StaggeredMassLight = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Mass|Staggered", meta = (ClampMin = "0.1"))
	double StaggeredMassMedium = 1.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Mass|Staggered", meta = (ClampMin = "0.1"))
	double StaggeredMassHeavy = 2.0;

	/** 蓄力期绘制施力方向/进度调试线（灰盒可读性，正式表现前临时）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hammer|Debug")
	bool bDrawChargeDebug = true;

private:
	/** 施力方向：镜头 Yaw 的水平前向（俯视角"鼠标地面瞄准"）。 */
	FVector ComputeStrikeDirection() const;

	/** 对前方最近的可推目标施加冲量并结算稳定度伤害。 */
	void ApplyImpulseToHitActors(const FVector& Direction);

	/** 按质量档查相对质量；bStaggered 时用失衡（脱锚）质量表。 */
	double GetMassValue(EVectorMassClass MassClass, bool bStaggered) const;

	/** 蓄力期调试线颜色随进度从绿变红。 */
	FLinearColor ComputeChargeDebugColor() const;
};
