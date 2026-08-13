// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VectorCharacterMovementComponent.generated.h"

/**
 * 冲量荒原平面世界移动组件：标准 CharacterMovement + 受控冲量队列。
 *
 * 移植自 MorphorbitCharacterMovementComponent（QueueWorldVelocityChange 平面化，
 * 删除径向重力/引力井/逃逸速度相关逻辑）。核心能力：装备/敌人可向拥有者排队
 * 一次世界空间 Δv，在下一帧正常 CalcVelocity（输入+地面制动）完成后注入，
 * 随后一帧即失效——不持续施力、不改摩擦/移动模式；高速冲量使用 1/120 s
 * 有限外层子步避免穿墙。
 */
UCLASS()
class VECTOR_API UVectorCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void StopMovementImmediately() override;

	/**
	 * 排队一次世界空间速度变化，在下一帧正常 CalcVelocity 完成输入和地面制动后消费。
	 *
	 * @param WorldDeltaVelocity 世界空间 Δv（cm/s），必须有限；与已排队值累加后仍须有限。
	 * @return true 表示已入队；非有限值或累加溢出返回 false，不产生任何变化。
	 */
	bool QueueWorldVelocityChange(const FVector& WorldDeltaVelocity);

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
	 * 由 QueueWorldVelocityChange 注入置 true，速度衰减到
	 * ImpulseDrivenMinSpeedCmPerSecond 以下或停止时清 false。
	 * 只有该状态下发生的高速碰撞才结算碰撞伤害（正常行走/站桩不伤友军，
	 * 对齐设计案"只有失衡或物理运动状态的敌人才会造成友军碰撞伤害"）。
	 */
	bool IsImpulseDriven() const { return bIsImpulseDriven; }

	/** 退出冲量驱动状态的剩余速度阈值，cm/s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Movement|Impulse", meta = (ClampMin = "0.0", Units = "cm/s"))
	double ImpulseDrivenMinSpeedCmPerSecond = 100.0;

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

private:
	/** 等待正常速度计算结算的一次性世界空间 Δv。 */
	FVector PendingWorldVelocityChange = FVector::ZeroVector;

	/** 最近一次 CalcVelocity 真实消费的世界空间 Δv。 */
	FVector AppliedWorldVelocityChangeThisStep = FVector::ZeroVector;

	/** 仅在受击速度仍沿初始方向为正时，要求 Walking 使用稳定的有限外层子步。 */
	FVector ActiveVelocityChangeDirection = FVector::ZeroVector;
	bool bUseVelocityChangeSubsteps = false;

	/** 冲量驱动状态（S03 碰撞连锁判定）。 */
	bool bIsImpulseDriven = false;
};
