// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemyController.h"

#include "Combat/VectorEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "NavigationSystem.h"
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
	constexpr double DropAttackMinimumHeightCm = 180.0;
	constexpr double DropAttackMaximumPlanarRangeCm = 1300.0;
	constexpr double DropAttackBaseHorizontalSpeedCmPerSecond = 3000.0;
	constexpr double DropAttackVerticalSpeedCmPerSecond = 220.0;
	constexpr double DropAttackWarmupSeconds = 0.65;
	constexpr double DropAttackCooldownSeconds = 4.0;
	constexpr double PathRefreshIntervalSeconds = 0.30;
	constexpr double PathGoalMovementThresholdCm = 110.0;
	constexpr double StuckSampleIntervalSeconds = 0.40;
	constexpr double StuckMovementThresholdCm = 18.0;
	constexpr double StuckRecoveryThresholdSeconds = 1.20;
	constexpr double RecoveryMoveDurationSeconds = 0.55;
	constexpr double RecoveryLateralDistanceCm = 260.0;
	constexpr double MinimumEngagementRadiusCm = 165.0;
	constexpr double EngagementRadiusStepCm = 25.0;
	constexpr double EngagementGoalAcceptanceRadiusCm = 65.0;
}

AVectorEnemyController::AVectorEnemyController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(
		TEXT("PathFollowingComponent")))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVectorEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledEnemy = Cast<AVectorEnemy>(InPawn);
	PathRefreshRemainingSeconds = 0.0;
	StuckSampleRemainingSeconds = StuckSampleIntervalSeconds;
	StuckAccumulatedSeconds = 0.0;
	RecoveryMoveRemainingSeconds = 0.0;
	LastStuckSampleLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	bHasPathGoal = false;
	const uint32 SlotIndex = InPawn ? InPawn->GetUniqueID() : 0u;
	// Golden-angle spacing keeps sequentially spawned enemies distributed around
	// the player instead of clustering adjacent object-name hashes.
	const double SlotAngleDegrees = FMath::Fmod(
		static_cast<double>(SlotIndex) * 137.507764, 360.0);
	EngagementAngleRadians = FMath::DegreesToRadians(SlotAngleDegrees);
	EngagementRadiusCm = MinimumEngagementRadiusCm
		+ static_cast<double>(SlotIndex % 3u) * EngagementRadiusStepCm;
	if (UCrowdFollowingComponent* CrowdFollowing =
		Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		CrowdFollowing->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
		CrowdFollowing->SetCrowdObstacleAvoidance(true);
		CrowdFollowing->SetCrowdSeparation(true);
		CrowdFollowing->SetCrowdSeparationWeight(6.0f);
		CrowdFollowing->SetCrowdCollisionQueryRange(520.0f);
		CrowdFollowing->SetCrowdPathOptimizationRange(1200.0f);
		CrowdFollowing->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
		CrowdFollowing->SetCrowdAnticipateTurns(true);
		CrowdFollowing->SetCrowdOptimizeTopology(true);
		UE_LOG(LogVectorEnemyAI, Log,
			TEXT("Enemy crowd steering configured: enemy=%s separation=YES avoidance=HIGH slotAngle=%.0f slotRadius=%.0f check=PASS"),
			*GetNameSafe(InPawn), FMath::RadiansToDegrees(EngagementAngleRadians),
			EngagementRadiusCm);
	}
}

void AVectorEnemyController::OnUnPossess()
{
	// 清理缓存防悬垂（Pawn 被销毁时 UnPossess 触发）。
	if (ControlledEnemy)
	{
		ControlledEnemy->SetAttackWarningPresentation(false);
	}
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
	DropAttackCooldownRemainingSeconds = FMath::Max(
		0.0, DropAttackCooldownRemainingSeconds - DeltaSeconds);
	PathFailureLogCooldownRemainingSeconds = FMath::Max(
		0.0, PathFailureLogCooldownRemainingSeconds - DeltaSeconds);
	PathRefreshRemainingSeconds = FMath::Max(
		0.0, PathRefreshRemainingSeconds - DeltaSeconds);
	RecoveryMoveRemainingSeconds = FMath::Max(
		0.0, RecoveryMoveRemainingSeconds - DeltaSeconds);

	if (bPreparingDropAttack)
	{
		if (ControlledEnemy->ShouldPauseAI())
		{
			bPreparingDropAttack = false;
			ControlledEnemy->SetAttackWarningPresentation(false);
			DropAttackCooldownRemainingSeconds = 1.0;
			UE_LOG(LogVectorEnemyAI, Log,
				TEXT("Enemy drop telegraph interrupted: enemy=%s"),
				*ControlledEnemy->GetName());
			return;
		}
		PausePathFollowing();
		DropAttackWarmupRemainingSeconds -= DeltaSeconds;
		if (DropAttackWarmupRemainingSeconds > 0.0)
		{
			return;
		}

		UVectorCharacterMovementComponent* Movement =
			ControlledEnemy->FindComponentByClass<UVectorCharacterMovementComponent>();
		const UVectorStabilityComponent* Stability =
			ControlledEnemy->FindComponentByClass<UVectorStabilityComponent>();
		const double Mass = Stability ? Stability->GetEffectivePhysicalMass() : 2.5;
		const double HorizontalSpeed = FMath::Clamp(
			FVectorImpactMath::ComputeMassAdjustedSpeed(
				DropAttackBaseHorizontalSpeedCmPerSecond, Mass),
			600.0, 1000.0);
		const FVector LaunchVelocity = LockedDropAttackDirection * HorizontalSpeed
			+ FVector::UpVector * DropAttackVerticalSpeedCmPerSecond;
		const bool bQueued = Movement
			&& Movement->QueueAirborneWorldVelocityOverride(LaunchVelocity);
		bPreparingDropAttack = false;
		ControlledEnemy->SetAttackWarningPresentation(false);
		DropAttackCooldownRemainingSeconds = DropAttackCooldownSeconds;
		UE_LOG(LogVectorEnemyAI, Log,
			TEXT("Enemy drop attack: enemy=%s mass=%.2f velocity=%s queued=%s"),
			*ControlledEnemy->GetName(), Mass, *LaunchVelocity.ToCompactString(),
			bQueued ? TEXT("OK") : TEXT("REJECTED"));
		return;
	}

	// 冲锋流程推进（Charger 型）。
	if (bCharging)
	{
		if (ChargeWarmupRemainingSeconds > 0.0)
		{
			// 前摇尚未发射时仍可被锤击/牵引打断；发射后的冲锋本身会进入
			// impulse-driven，不能用同一条件把自己的冲锋立即取消。
			if (ControlledEnemy->ShouldPauseAI())
			{
				ControlledEnemy->SetAttackWarningPresentation(false);
				bCharging = false;
				ChargeWarmupRemainingSeconds = 0.0;
				ChargeActiveRemainingSeconds = 0.0;
				ChargeCooldownRemainingSeconds = 1.0;
				UE_LOG(LogVectorEnemyAI, Log, TEXT("Charger %s warmup interrupted by physical state"),
					*ControlledEnemy->GetName());
				return;
			}
			ChargeWarmupRemainingSeconds -= DeltaSeconds;
			// 预警期间原地停：暂停路径跟随（禁用 StopMovement——会清冲量）。
			PausePathFollowing();
			if (ChargeWarmupRemainingSeconds <= 0.0)
			{
				// 预警结束：锁定方向并施加高速冲量。
				ControlledEnemy->SetAttackWarningPresentation(false);
				ChargeDirection = ChargeDirection.GetSafeNormal();
				if (UVectorCharacterMovementComponent* Movement = ControlledEnemy->FindComponentByClass<UVectorCharacterMovementComponent>())
				{
					Movement->QueueDirectionalVelocityOverride(ChargeDirection, ChargeSpeedCmPerSecond);
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
				ControlledEnemy->SetAttackWarningPresentation(false);
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
		&& !ControlledEnemy->ShouldPauseAI()
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

	// A path request every frame continuously restarts local steering at seams and
	// makes crowds look as if they are glued to grid edges. Sample actual travel,
	// then briefly route sideways when a valid moving request makes no progress.
	StuckSampleRemainingSeconds -= DeltaSeconds;
	if (StuckSampleRemainingSeconds <= 0.0)
	{
		const FVector CurrentLocation = ControlledEnemy->GetActorLocation();
		const double TravelDistance = FVector::Dist2D(CurrentLocation, LastStuckSampleLocation);
		const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
		const bool bExpectedToMove = PathFollowing
			&& PathFollowing->GetStatus() == EPathFollowingStatus::Moving
			&& FVector::Dist2D(CurrentLocation, PlayerPawn->GetActorLocation())
				> MoveAcceptanceRadiusCm * 1.5;
		StuckAccumulatedSeconds = bExpectedToMove && TravelDistance < StuckMovementThresholdCm
			? StuckAccumulatedSeconds + StuckSampleIntervalSeconds
			: 0.0;
		LastStuckSampleLocation = CurrentLocation;
		StuckSampleRemainingSeconds = StuckSampleIntervalSeconds;

		if (StuckAccumulatedSeconds >= StuckRecoveryThresholdSeconds)
		{
			int32 NearbyPawnBlockers = 0;
			for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			{
				if (*It != ControlledEnemy
					&& FVector::DistSquared2D(It->GetActorLocation(), CurrentLocation)
						<= FMath::Square(260.0))
				{
					++NearbyPawnBlockers;
				}
			}
			const FVector TowardPlayer = (PlayerPawn->GetActorLocation() - CurrentLocation).GetSafeNormal2D();
			const FVector Lateral(-TowardPlayer.Y, TowardPlayer.X, 0.0);
			const double Side = (RecoveryAttemptCount++ % 2 == 0) ? 1.0 : -1.0;
			const FVector RawRecoveryPoint = CurrentLocation
				+ Lateral * Side * RecoveryLateralDistanceCm
				+ TowardPlayer * 70.0;
			FNavLocation ProjectedRecoveryPoint;
			UNavigationSystemV1* NavigationSystem =
				FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			const bool bProjected = NavigationSystem
				&& NavigationSystem->ProjectPointToNavigation(
					RawRecoveryPoint, ProjectedRecoveryPoint, FVector(220.0, 220.0, 180.0));
			if (bProjected)
			{
				MoveToLocation(ProjectedRecoveryPoint.Location, 45.0, true, true);
				RecoveryMoveRemainingSeconds = RecoveryMoveDurationSeconds;
				PathRefreshRemainingSeconds = RecoveryMoveDurationSeconds;
				UE_LOG(LogVectorEnemyAI, Log,
					TEXT("Enemy path recovery: enemy=%s travel=%.0f stuck=%.1fs nearbyPawns=%d side=%+.0f point=%s check=QUEUED"),
					*ControlledEnemy->GetName(), TravelDistance, StuckAccumulatedSeconds,
					NearbyPawnBlockers, Side, *ProjectedRecoveryPoint.Location.ToCompactString());
			}
			else
			{
				UE_LOG(LogVectorEnemyAI, Warning,
					TEXT("Enemy path recovery: enemy=%s point=%s check=NO_NAV"),
					*ControlledEnemy->GetName(), *RawRecoveryPoint.ToCompactString());
			}
			StuckAccumulatedSeconds = 0.0;
		}
	}

	if (RecoveryMoveRemainingSeconds > 0.0)
	{
		return;
	}

	const FVector ToPlayer = PlayerPawn->GetActorLocation() - ControlledEnemy->GetActorLocation();
	if (ControlledEnemy->Archetype != EVectorEnemyArchetype::ChargerRammer
		&& DropAttackCooldownRemainingSeconds <= 0.0
		&& ToPlayer.Z <= -DropAttackMinimumHeightCm
		&& ToPlayer.SizeSquared2D() <= FMath::Square(DropAttackMaximumPlanarRangeCm))
	{
		bPreparingDropAttack = true;
		DropAttackWarmupRemainingSeconds = DropAttackWarmupSeconds;
		LockedDropAttackDirection = ToPlayer.GetSafeNormal2D();
		ControlledEnemy->SetAttackWarningPresentation(true);
		PausePathFollowing();
		UE_LOG(LogVectorEnemyAI, Log,
			TEXT("Enemy drop telegraph: enemy=%s height=%.0f planar=%.0f duration=%.2fs direction=%s"),
			*ControlledEnemy->GetName(), -ToPlayer.Z, ToPlayer.Size2D(),
			DropAttackWarmupSeconds, *LockedDropAttackDirection.ToCompactString());
		return;
	}

	// 恢复被暂停的路径跟随，再继续追击。
	ResumePathFollowingIfPaused();
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector EngagementOffset(
		FMath::Cos(EngagementAngleRadians) * EngagementRadiusCm,
		FMath::Sin(EngagementAngleRadians) * EngagementRadiusCm,
		0.0);
	const FVector RawPursuitGoal = PlayerLocation + EngagementOffset;
	const bool bGoalMoved = !bHasPathGoal
		|| FVector::DistSquared2D(RawPursuitGoal, LastPathGoal)
			>= FMath::Square(PathGoalMovementThresholdCm);
	const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
	const bool bNeedsRequest = !PathFollowing
		|| PathFollowing->GetStatus() == EPathFollowingStatus::Idle
		|| PathFollowing->GetStatus() == EPathFollowingStatus::Waiting;
	if (PathRefreshRemainingSeconds > 0.0 && !bGoalMoved && !bNeedsRequest)
	{
		return;
	}
	FNavLocation ProjectedPursuitGoal;
	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bProjectedPursuitGoal = NavigationSystem
		&& NavigationSystem->ProjectPointToNavigation(
			RawPursuitGoal, ProjectedPursuitGoal, FVector(240.0, 240.0, 180.0));
	const FVector PursuitGoal = bProjectedPursuitGoal
		? ProjectedPursuitGoal.Location : PlayerLocation;
	const EPathFollowingRequestResult::Type PathRequest = MoveToLocation(
		PursuitGoal, EngagementGoalAcceptanceRadiusCm,
		false, true, false, true, nullptr, true);
	PathRefreshRemainingSeconds = PathRefreshIntervalSeconds;
	LastPathGoal = RawPursuitGoal;
	bHasPathGoal = PathRequest != EPathFollowingRequestResult::Failed;
	if (PathRequest == EPathFollowingRequestResult::Failed
		&& PathFailureLogCooldownRemainingSeconds <= 0.0)
	{
		PathFailureLogCooldownRemainingSeconds = 1.0;
		UE_LOG(LogVectorEnemyAI, Warning,
			TEXT("Enemy path request failed: enemy=%s from=%s player=%s slot=%s projected=%s deltaZ=%.0f"),
			*ControlledEnemy->GetName(),
			*ControlledEnemy->GetActorLocation().ToCompactString(),
			*PlayerPawn->GetActorLocation().ToCompactString(),
			*RawPursuitGoal.ToCompactString(),
			bProjectedPursuitGoal ? TEXT("YES") : TEXT("no"), ToPlayer.Z);
	}
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
	ControlledEnemy->SetAttackWarningPresentation(true);
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
