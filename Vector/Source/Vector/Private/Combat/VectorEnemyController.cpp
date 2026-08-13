// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemyController.h"

#include "Combat/VectorEnemy.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEnemyAI, Log, All);

namespace
{
	constexpr double ChargeWarmupSeconds = 0.5;
	constexpr double ChargeActiveSeconds = 0.6;
	constexpr double ChargeCooldownSeconds = 4.0;
	constexpr double ChargeSpeedCmPerSecond = 1600.0;
	constexpr double ChargeTriggerRangeCm = 900.0;
	constexpr double MoveAcceptanceRadiusCm = 120.0;
}

AVectorEnemyController::AVectorEnemyController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVectorEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledEnemy = Cast<AVectorEnemy>(InPawn);
}

void AVectorEnemyController::OnUnPossess()
{
	// 清理缓存防悬垂（Pawn 被销毁时 UnPossess 触发）。
	ControlledEnemy = nullptr;
	Super::OnUnPossess();
}

void AVectorEnemyController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ControlledEnemy)
	{
		return;
	}

	APawn* PlayerPawn = FindPlayerPawn();

	// 冲锋流程推进（Charger 型）。
	if (bCharging)
	{
		if (ChargeWarmupRemainingSeconds > 0.0)
		{
			ChargeWarmupRemainingSeconds -= DeltaSeconds;
			// 预警期间原地停：暂停路径跟随（禁用 StopMovement——会清冲量）。
			PausePathFollowing();
			if (ChargeWarmupRemainingSeconds <= 0.0)
			{
				// 预警结束：锁定方向并施加高速冲量。
				ChargeDirection = ChargeDirection.GetSafeNormal();
				if (UVectorCharacterMovementComponent* Movement = ControlledEnemy->FindComponentByClass<UVectorCharacterMovementComponent>())
				{
					Movement->QueueWorldVelocityChange(ChargeDirection * ChargeSpeedCmPerSecond);
					UE_LOG(LogVectorEnemyAI, Log, TEXT("Charger %s launches at %.0f cm/s along %s"),
						*ControlledEnemy->GetName(), ChargeSpeedCmPerSecond, *ChargeDirection.ToCompactString());
				}
				ChargeActiveRemainingSeconds = ChargeActiveSeconds;
			}
		}
		else if (ChargeActiveRemainingSeconds > 0.0)
		{
			ChargeActiveRemainingSeconds -= DeltaSeconds;
			// 冲锋中：AI 让路（冲量驱动，不 MoveTo、不清速度）。
			PausePathFollowing();
			if (ChargeActiveRemainingSeconds <= 0.0)
			{
				bCharging = false;
				ChargeCooldownRemainingSeconds = ChargeCooldownSeconds;
				UE_LOG(LogVectorEnemyAI, Log, TEXT("Charger %s charge finished"), *ControlledEnemy->GetName());
			}
		}
		return;
	}

	if (ChargeCooldownRemainingSeconds > 0.0)
	{
		ChargeCooldownRemainingSeconds -= DeltaSeconds;
	}

	// 尝试触发冲锋（仅 Charger 型，玩家在射程内，不在冷却）。
	if (ControlledEnemy->Archetype == EVectorEnemyArchetype::ChargerRammer
		&& ChargeCooldownRemainingSeconds <= 0.0
		&& PlayerPawn
		&& FVector::Dist2D(ControlledEnemy->GetActorLocation(), PlayerPawn->GetActorLocation()) <= ChargeTriggerRangeCm)
	{
		TriggerCharge();
		return;
	}

	// 常态追击：失衡/倒地/冲量驱动期间 AI 让路（物理状态优先）。
	if (!PlayerPawn || ControlledEnemy->ShouldPauseAI())
	{
		PausePathFollowing();
		return;
	}

	// 恢复被暂停的路径跟随，再继续追击。
	ResumePathFollowingIfPaused();
	MoveToActor(PlayerPawn, MoveAcceptanceRadiusCm);
}

void AVectorEnemyController::TriggerCharge()
{
	if (bCharging || !ControlledEnemy)
	{
		return;
	}

	APawn* PlayerPawn = FindPlayerPawn();
	if (!PlayerPawn)
	{
		return;
	}

	// 预警：锁定朝向玩家方向，0.5s 后执行（期间敌人停止，可读）。
	ChargeDirection = (PlayerPawn->GetActorLocation() - ControlledEnemy->GetActorLocation()).GetSafeNormal2D();
	ChargeWarmupRemainingSeconds = ChargeWarmupSeconds;
	bCharging = true;
	UE_LOG(LogVectorEnemyAI, Log, TEXT("Charger %s warmup started toward %s"),
		*ControlledEnemy->GetName(), *PlayerPawn->GetName());
}

APawn* AVectorEnemyController::FindPlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->GetPawn())
		{
			return PC->GetPawn();
		}
	}
	return nullptr;
}

void AVectorEnemyController::PausePathFollowing()
{
	// 只暂停路径跟随，禁用 StopMovement——它会调用 MovementComponent::StopMovementImmediately，
	// 清空受控冲量（被推飞的"弹药"会瞬间停住）。
	// VelocityMode 必须用 Keep：默认 Reset 会把移动组件速度清零，效果等同清冲量。
	if (UPathFollowingComponent* PathFollowing = GetPathFollowingComponent())
	{
		if (PathFollowing->GetStatus() == EPathFollowingStatus::Moving
			|| PathFollowing->GetStatus() == EPathFollowingStatus::Waiting)
		{
			PathFollowing->PauseMove(FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Keep);
		}
	}
}

void AVectorEnemyController::ResumePathFollowingIfPaused()
{
	if (UPathFollowingComponent* PathFollowing = GetPathFollowingComponent())
	{
		if (PathFollowing->GetStatus() == EPathFollowingStatus::Paused)
		{
			PathFollowing->ResumeMove();
		}
	}
}
