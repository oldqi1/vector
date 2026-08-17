// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VectorEnemyController.generated.h"

/**
 * 敌人 AI 控制器（S04，NavMesh 寻路 + 冲锋行为）。
 *
 * - 常态：MoveToActor 玩家（导航网格自动绕墙/脱困）；
 * - 失衡/倒地/被冲量驱动：暂停 MoveTo（StopMovement），物理状态优先；
 * - 角槌兽：周期性触发冲锋（预警 0.5s → 高速冲量冲向玩家 → 冷却）。
 */
UCLASS()
class VECTOR_API AVectorEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	AVectorEnemyController(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/** 冲锋触发（由拥有者敌人调用，仅 Charger 型有意义）。 */
	void TriggerCharge();

	/** 冲锋是否处于预警/冲锋中。 */
	bool IsCharging() const { return bCharging; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	/** 目标玩家 Pawn（当前世界第一个玩家控制器所控 Pawn）。 */
	APawn* FindPlayerPawn() const;

	/** 暂停路径跟随（不清冲量/速度；禁用 StopMovement 以防打断被推飞的"弹药"）。 */
	void PausePathFollowing();

	/** 若路径跟随处于暂停则恢复。 */
	void ResumePathFollowingIfPaused();

	/** 被控制的敌人（缓存）。 */
	TObjectPtr<class AVectorEnemy> ControlledEnemy;

	/** 是否处于冲锋流程。 */
	bool bCharging = false;

	/** 冲锋预警剩余时长，s（预警后执行冲量）。 */
	double ChargeWarmupRemainingSeconds = 0.0;

	/** 冲锋持续剩余时长，s。 */
	double ChargeActiveRemainingSeconds = 0.0;

	/** 冲锋冷却剩余时长，s。 */
	double ChargeCooldownRemainingSeconds = 0.0;

	/** 冲锋方向（预警结束时锁定）。 */
	FVector ChargeDirection = FVector::ForwardVector;

	double DropAttackCooldownRemainingSeconds = 0.0;
	double PathFailureLogCooldownRemainingSeconds = 0.0;
	double PathRefreshRemainingSeconds = 0.0;
	double StuckSampleRemainingSeconds = 0.0;
	double StuckAccumulatedSeconds = 0.0;
	double RecoveryMoveRemainingSeconds = 0.0;
	double EngagementAngleRadians = 0.0;
	double EngagementRadiusCm = 185.0;
	FVector LastPathGoal = FVector::ZeroVector;
	FVector LastStuckSampleLocation = FVector::ZeroVector;
	int32 RecoveryAttemptCount = 0;
	bool bHasPathGoal = false;
	bool bPreparingDropAttack = false;
	double DropAttackWarmupRemainingSeconds = 0.0;
	FVector LockedDropAttackDirection = FVector::ForwardVector;
};
