// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VectorCharacterMovementComponent.generated.h"

/**
 * 冲量荒原三维角色移动组件：标准 CharacterMovement + 受控冲量/空中发射队列。
 *
 * 移植自 MorphorbitCharacterMovementComponent（删除径向重力世界规则，
 * 删除径向重力/引力井/逃逸速度相关逻辑）。核心能力：装备/碰撞系统可向拥有者排队
 * 一次世界空间目标速度，在下一帧正常 CalcVelocity 后覆盖；同一帧多次裁决采用
 * “后一次基于前一次结果继续解算、最后结果覆盖”，禁止把多个目标速度相加。
 * 随后一帧即失效——不持续施力；标准 Launch 专门负责进入 Falling，
 * 高速地面冲量使用 1/120 s 有限外层子步避免穿墙。
 */
UCLASS()
class VECTOR_API UVectorCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void StopMovementImmediately() override;

	/**
	 * 排队一次完整世界空间目标速度，在下一帧正常 CalcVelocity 后覆盖。
	 * 这不是 Δv；命名明确区分后可避免碰撞减速把负 Δv 错当成反向目标速度。
	 * 同帧再次调用采用 last-write-wins，调用方可通过 GetEffectiveVelocityForPendingStep
	 * 读取上一次待应用结果后继续解算，不会把两个目标速度相加制造能量。
	 */
	bool QueueWorldVelocityOverride(const FVector& WorldVelocity);

	/**
	 * 设置沿指定方向的目标速度，保留垂直于该方向的已有速度分量。
	 * 锤击、扑击和冲锋使用此入口；碰撞解算使用完整速度覆盖入口。
	 */
	bool QueueDirectionalVelocityOverride(const FVector& WorldDirection, double TargetSpeedCmPerSecond);

	/**
	 * Queue an actual CharacterMovement launch. Unlike the generic CalcVelocity
	 * override, UE consumes this at the end of movement and enters Falling in
	 * the same canonical step, so vertical tools cannot be flattened by a
	 * Walking/Falling timing transition.
	 */
	bool QueueAirborneWorldVelocityOverride(const FVector& WorldVelocity);
	bool QueueDirectionalAirborneVelocityOverride(
		const FVector& WorldDirection,
		double TargetSpeedCmPerSecond);

	/** Preserve traversal exit velocity with temporarily reduced ground braking. */
	void BeginMomentumCarry(double DurationSeconds);

	/** 当前速度，若本帧已有待应用覆盖则返回覆盖值（供连续碰撞解算）。 */
	FVector GetEffectiveVelocityForPendingStep() const;

	/** 清除尚未消费的一次性世界 Δv（供终局、显式重置与生命周期清理使用）。 */
	void ClearQueuedWorldVelocityChanges();

	/** 最近一次 CalcVelocity 真实消费的世界空间 Δv；仅供诊断/表现读取。 */
	FVector GetAppliedWorldVelocityChangeThisStep() const
	{
		return AppliedWorldVelocityChangeThisStep;
	}

	/**
	 * 是否处于"冲量驱动的物理运动"状态（S03 碰撞连锁判定用）。
	 *
	 * 由受控速度覆盖注入置 true，速度衰减到
	 * ImpulseDrivenMinSpeedCmPerSecond 以下或停止时清 false。
	 * 只有该状态下发生的高速碰撞才结算碰撞伤害（正常行走/站桩不伤友军，
	 * 对齐设计案"只有失衡或物理运动状态的敌人才会造成友军碰撞伤害"）。
	 */
	bool IsImpulseDriven() const { return bIsImpulseDriven; }

	/** 落地状态退出冲量驱动的剩余速度阈值，cm/s；Falling 期间保持物理状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Movement|Impulse", meta = (ClampMin = "0.0", Units = "cm/s"))
	double ImpulseDrivenMinSpeedCmPerSecond = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Movement|Impulse", meta = (ClampMin = "0.0"))
	double MomentumCarryFriction = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Movement|Impulse", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	double MomentumCarryBrakingDeceleration = 120.0;

protected:
	virtual void CalcVelocity(
		float DeltaTime,
		float Friction,
		bool bFluid,
		float BrakingDeceleration) override;
	virtual void PhysWalking(float DeltaTime, int32 Iterations) override;
	virtual void HandleImpact(
		const FHitResult& Hit,
		float TimeSlice,
		const FVector& MoveDelta) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual bool HandlePendingLaunch() override;

	/**
	 * AI 移动请求入口（PathFollowing 每帧调用）。
	 *
	 * 冲量驱动期间忽略 AI 的移动请求：被推飞的"弹药"不受寻路干扰，
	 * 否则 AI 下一帧 RequestDirectMove 会把注入的冲量速度覆盖掉
	 * （2026-08-14 修复：移动中的怪被满蓄推只顿一下）。
	 */
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;

private:
	/** 等待正常速度计算结算的一次性世界空间目标速度。 */
	FVector PendingWorldVelocityOverride = FVector::ZeroVector;
	bool bHasPendingWorldVelocityOverride = false;

	/** 最近一次 CalcVelocity 真实消费的世界空间 Δv。 */
	FVector AppliedWorldVelocityChangeThisStep = FVector::ZeroVector;

	/** 仅在受击速度仍沿初始方向为正时，要求 Walking 使用稳定的有限外层子步。 */
	FVector ActiveVelocityChangeDirection = FVector::ZeroVector;
	bool bUseVelocityChangeSubsteps = false;

	/** 冲量驱动状态（S03 碰撞连锁判定）。 */
	bool bIsImpulseDriven = false;
	double MomentumCarrySecondsRemaining = 0.0;
};
