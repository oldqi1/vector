// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stability/VectorStabilityTypes.h"
#include "VectorStabilityComponent.generated.h"

/** 稳定度归零触发失衡时广播（无参，蓝图可绑定）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVectorStabilityStaggeredSignature);

/**
 * 稳定/失衡运行时组件（A1+A2 移植落地）。
 *
 * 逻辑本体在 FVectorStabilityLedger（纯 C++ 可测），本组件只做：
 * 1. UPROPERTY 配置（数值、质量三档系数、碰撞类型系数）在 BeginPlay 同步到账本；
 * 2. Tick 驱动账本状态推进；
 * 3. 暴露 Blueprint 调用/查询入口与 OnStaggered 事件。
 *
 * 不含装备动作阶段互斥（红线 DEBT-01：动作互斥抽象留到装备 Story）。
 */
UCLASS(ClassGroup = (Vector), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorStabilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorStabilityComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 结算一次撞击失衡（基线公式：相对速度 → 基础失衡量，再 × 质量 × 碰撞类型）。
	 *
	 * @param BaseStaggerDamage 基础失衡量（建议来自 FVectorImpactMath::ComputeStaggerDamage）。
	 * @param MassClass         质量三档。
	 * @param ImpactType        碰撞类型。
	 * @return 实际施加的稳定度伤害（含乘数）；无效输入返回零。
	 */
	UFUNCTION(BlueprintCallable, Category = "Vector|Stability")
	double ReceiveImpactHit(
		double BaseStaggerDamage,
		EVectorMassClass InMassClass = EVectorMassClass::Medium,
		EVectorImpactType ImpactType = EVectorImpactType::Body);

	/** 恢复首局账本（稳定度回满、回 Stable）。 */
	UFUNCTION(BlueprintCallable, Category = "Vector|Stability")
	void ResetStability();

	UFUNCTION(BlueprintPure, Category = "Vector|Stability")
	double GetStability() const { return Ledger.Stability; }

	UFUNCTION(BlueprintPure, Category = "Vector|Stability")
	EVectorStabilityState GetState() const { return Ledger.State; }

	/** 是否处于失衡/倒地/起身任一恢复状态。 */
	UFUNCTION(BlueprintPure, Category = "Vector|Stability")
	bool IsStaggered() const { return Ledger.IsStaggered(); }

	/** 本目标的质量三档（决定被推的难易与碰撞伤害系数）。 */
	UFUNCTION(BlueprintPure, Category = "Vector|Stability")
	EVectorMassClass GetMassClass() const { return MassClass; }

	/** 本目标在指定质量档下的质量系数（查表，原型期默认 1.0 收敛）。 */
	UFUNCTION(BlueprintPure, Category = "Vector|Stability")
	double GetMassMultiplierByClass(EVectorMassClass InMassClass) const { return GetMassMultiplier(InMassClass); }

	UFUNCTION(BlueprintPure, Category = "Vector|Stability")
	double GetStateSecondsRemaining() const { return Ledger.StateSecondsRemaining; }

	/** 稳定度归零触发失衡时广播；只读事实，不承载表现。 */
	UPROPERTY(BlueprintAssignable, Category = "Vector|Stability")
	FVectorStabilityStaggeredSignature OnStaggered;

	// ---- 账本数值配置（默认值对齐 Morphorbit 常量，可 Details 调校） ----

	/** 质量三档（决定被推难易与碰撞伤害系数）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability")
	EVectorMassClass MassClass = EVectorMassClass::Medium;

	/** 稳定度上限，归零后重置（移植 MaximumStability=60）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "1.0"))
	double MaximumStability = 60.0;

	/** 基础失衡量下限（移植 20）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "0.0"))
	double MinimumBaseStaggerDamage = 20.0;

	/** 基础失衡量上限（移植 30）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "0.0"))
	double MaximumBaseStaggerDamage = 30.0;

	/** 弱点/开放窗口乘数（移植 1.5）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "0.0"))
	double WeakpointStaggerMultiplier = 1.5;

	/** 失衡硬直时长，单位 s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "0.0", Units = "s"))
	double UnbalancedDurationSeconds = 0.20;

	/** 倒地时长，单位 s（移植 StaggeredRecoveryDurationSeconds=1.60）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "0.0", Units = "s"))
	double DownedDurationSeconds = 1.60;

	/** 起身时长，单位 s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability", meta = (ClampMin = "0.0", Units = "s"))
	double RisingDurationSeconds = 0.40;

	// ---- 质量三档系数（碰撞伤害：重物一旦动起来威力极高） ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability|Mass", meta = (ClampMin = "0.0"))
	double LightMassMultiplier = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability|Mass", meta = (ClampMin = "0.0"))
	double MediumMassMultiplier = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability|Mass", meta = (ClampMin = "0.0"))
	double HeavyMassMultiplier = 1.4;

	// ---- 碰撞类型系数（基线公式预留；撞墙/落地语义待 S03 碰撞连锁接入） ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability|Collision", meta = (ClampMin = "0.0"))
	double BodyImpactMultiplier = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability|Collision", meta = (ClampMin = "0.0"))
	double WallImpactMultiplier = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Stability|Collision", meta = (ClampMin = "0.0"))
	double GroundImpactMultiplier = 1.0;

private:
	/** 把 UPROPERTY 配置同步到纯账本；BeginPlay 与首次结算前幂等调用。 */
	void ApplyConfiguration();

	double GetMassMultiplier(EVectorMassClass InMassClass) const;
	double GetImpactTypeMultiplier(EVectorImpactType ImpactType) const;

	/** 稳定度账本本体（纯 C++，可被 Automation 直接测）。 */
	FVectorStabilityLedger Ledger;

	bool bConfigurationApplied = false;
};
