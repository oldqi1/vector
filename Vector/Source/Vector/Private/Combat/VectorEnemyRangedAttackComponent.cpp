// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemyRangedAttackComponent.h"

#include "Boss/VectorKineticOrb.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorTestDummy.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEnemyRanged, Log, All);

UVectorEnemyRangedAttackComponent::UVectorEnemyRangedAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	ProjectileClass = AVectorKineticOrb::StaticClass();
}

void UVectorEnemyRangedAttackComponent::ConfigurePattern(
	const EVectorEnemyRangedPattern NewPattern)
{
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(GetOwner()))
	{
		Dummy->SetAttackWarningPresentation(false);
	}
	Pattern = NewPattern;
	Phase = EPhase::Idle;
	PhaseSecondsRemaining = 0.0;
	switch (Pattern)
	{
	case EVectorEnemyRangedPattern::ArcWeakHoming:
		AttackRangeCm = 1150.0;
		WarmupSeconds = 0.70;
		CooldownSeconds = 3.20;
		ProjectileSpeedCmPerSecond = 620.0;
		ProjectileLifetimeSeconds = 4.0;
		GuidanceSeconds = 1.80;
		GuidanceTurnRateDegreesPerSecond = 80.0;
		VolleySpreadDegrees = 0.0;
		break;
	case EVectorEnemyRangedPattern::CorrosionVolley:
		AttackRangeCm = 1050.0;
		WarmupSeconds = 0.55;
		CooldownSeconds = 2.40;
		ProjectileSpeedCmPerSecond = 760.0;
		ProjectileLifetimeSeconds = 2.8;
		GuidanceSeconds = 0.0;
		GuidanceTurnRateDegreesPerSecond = 0.0;
		VolleySpreadDegrees = 14.0;
		break;
	case EVectorEnemyRangedPattern::None:
	default:
		AttackRangeCm = 0.0;
		WarmupSeconds = 0.0;
		CooldownSeconds = 0.0;
		ProjectileSpeedCmPerSecond = 0.0;
		ProjectileLifetimeSeconds = 0.0;
		GuidanceSeconds = 0.0;
		GuidanceTurnRateDegreesPerSecond = 0.0;
		VolleySpreadDegrees = 0.0;
		break;
	}
	SetComponentTickEnabled(Pattern != EVectorEnemyRangedPattern::None);
	UE_LOG(LogVectorEnemyRanged, Log,
		TEXT("Enemy ranged pattern configured: owner=%s pattern=%s range=%.0f warmup=%.2f cooldown=%.2f projectileSpeed=%.0f"),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Pattern), AttackRangeCm,
		WarmupSeconds, CooldownSeconds, ProjectileSpeedCmPerSecond);
}

bool UVectorEnemyRangedAttackComponent::IsCommittingAttack() const
{
	return Phase == EPhase::Warmup;
}

void UVectorEnemyRangedAttackComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner || Pattern == EVectorEnemyRangedPattern::None)
	{
		return;
	}
	if (const UVectorHealthComponent* Health =
		Owner->FindComponentByClass<UVectorHealthComponent>(); Health && Health->IsDead())
	{
		CancelWarmup(TEXT("OWNER_DEAD"));
		return;
	}
	const UVectorCharacterMovementComponent* Movement =
		Owner->FindComponentByClass<UVectorCharacterMovementComponent>();
	const UVectorStabilityComponent* Stability =
		Owner->FindComponentByClass<UVectorStabilityComponent>();
	if ((Movement && Movement->IsImpulseDriven())
		|| (Stability && Stability->IsStaggered()))
	{
		CancelWarmup(Movement && Movement->IsImpulseDriven()
			? TEXT("EXTERNAL_IMPULSE") : TEXT("STAGGER"));
		return;
	}

	APawn* PlayerPawn = FindPlayerPawn();
	if (Phase == EPhase::Warmup)
	{
		if (!PlayerPawn)
		{
			CancelWarmup(TEXT("TARGET_LOST"));
			return;
		}
		DrawTelegraph(PlayerPawn);
		PhaseSecondsRemaining -= FMath::Max(0.0f, DeltaTime);
		if (PhaseSecondsRemaining <= 0.0)
		{
			ReleaseProjectiles(PlayerPawn);
		}
		return;
	}
	if (Phase == EPhase::Cooldown)
	{
		PhaseSecondsRemaining -= FMath::Max(0.0f, DeltaTime);
		if (PhaseSecondsRemaining <= 0.0)
		{
			Phase = EPhase::Idle;
		}
		return;
	}
	if (PlayerPawn && FVector::Dist2D(
		Owner->GetActorLocation(), PlayerPawn->GetActorLocation()) <= AttackRangeCm)
	{
		BeginWarmup(PlayerPawn);
	}
}

void UVectorEnemyRangedAttackComponent::BeginWarmup(APawn* PlayerPawn)
{
	if (!PlayerPawn || !GetOwner())
	{
		return;
	}
	Phase = EPhase::Warmup;
	PhaseSecondsRemaining = FMath::Max(0.01, WarmupSeconds);
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(GetOwner()))
	{
		Dummy->SetAttackWarningPresentation(true);
	}
	UE_LOG(LogVectorEnemyRanged, Log,
		TEXT("Enemy ranged telegraph: owner=%s pattern=%s target=%s duration=%.2fs tracking=CONTINUOUS"),
		*GetOwner()->GetName(), *UEnum::GetValueAsString(Pattern),
		*PlayerPawn->GetName(), PhaseSecondsRemaining);
}

void UVectorEnemyRangedAttackComponent::ReleaseProjectiles(APawn* PlayerPawn)
{
	AActor* Owner = GetOwner();
	if (!Owner || !PlayerPawn)
	{
		CancelWarmup(TEXT("INVALID_RELEASE"));
		return;
	}
	const FVector BaseDirection =
		(PlayerPawn->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	const bool bWeakHoming = Pattern == EVectorEnemyRangedPattern::ArcWeakHoming;
	const int32 ProjectileCount = bWeakHoming
		? 1 : ((AttackSequenceIndex++ % 2 == 0) ? 1 : 3);
	int32 SpawnedCount = 0;
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const double Alpha = ProjectileCount <= 1
			? 0.5 : static_cast<double>(Index) / (ProjectileCount - 1);
		const double AngleDegrees = FMath::Lerp(
			-VolleySpreadDegrees, VolleySpreadDegrees, Alpha);
		const FVector Direction = BaseDirection
			.RotateAngleAxis(AngleDegrees, FVector::UpVector).GetSafeNormal2D();
		if (SpawnProjectile(PlayerPawn, Direction, bWeakHoming))
		{
			++SpawnedCount;
		}
	}
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(Owner))
	{
		Dummy->SetAttackWarningPresentation(false);
	}
	Phase = EPhase::Cooldown;
	PhaseSecondsRemaining = FMath::Max(0.0, CooldownSeconds);
	UE_LOG(LogVectorEnemyRanged, Log,
		TEXT("Enemy ranged release: owner=%s pattern=%s projectiles=%d/%d target=%s speed=%.0f guidance=%.2fs check=%s"),
		*Owner->GetName(), *UEnum::GetValueAsString(Pattern), SpawnedCount,
		ProjectileCount, *PlayerPawn->GetName(), ProjectileSpeedCmPerSecond,
		GuidanceSeconds, SpawnedCount == ProjectileCount ? TEXT("PASS") : TEXT("FAIL"));
}

bool UVectorEnemyRangedAttackComponent::SpawnProjectile(
	APawn* PlayerPawn,
	const FVector& Direction,
	const bool bWeakHoming)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !ProjectileClass || Direction.IsNearlyZero())
	{
		return false;
	}
	const FVector SpawnLocation = Owner->GetActorLocation()
		+ Direction * 115.0 + FVector(0.0, 0.0, 45.0);
	FActorSpawnParameters Parameters;
	Parameters.Owner = Owner;
	Parameters.Instigator = Cast<APawn>(Owner);
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AVectorKineticOrb* Projectile = World->SpawnActor<AVectorKineticOrb>(
		ProjectileClass, SpawnLocation, Direction.Rotation(), Parameters);
	if (!Projectile)
	{
		return false;
	}
	Projectile->ConfigureProjectilePresentation(
		bWeakHoming ? FLinearColor(0.12f, 0.75f, 1.0f)
			: FLinearColor(0.2f, 1.0f, 0.08f),
		ProjectileLifetimeSeconds);
	return bWeakHoming
		? Projectile->LaunchWeakHoming(
			PlayerPawn, ProjectileSpeedCmPerSecond, GuidanceSeconds,
			GuidanceTurnRateDegreesPerSecond, Owner)
		: Projectile->Launch(Direction, ProjectileSpeedCmPerSecond, Owner);
}

void UVectorEnemyRangedAttackComponent::CancelWarmup(const TCHAR* Reason)
{
	// External motion may pause an existing cooldown, but must never erase it.
	// Otherwise a successful interrupt is punished by an immediate new telegraph
	// when the target lands.
	if (Phase != EPhase::Warmup)
	{
		return;
	}
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(GetOwner()))
	{
		Dummy->SetAttackWarningPresentation(false);
	}
	Phase = EPhase::Cooldown;
	PhaseSecondsRemaining = FMath::Max(0.0,
		FMath::Min(InterruptedRetrySeconds, CooldownSeconds));
	UE_LOG(LogVectorEnemyRanged, Log,
		TEXT("Enemy ranged interrupted: owner=%s pattern=%s reason=%s retry=%.2fs"),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Pattern),
		Reason ? Reason : TEXT("UNKNOWN"), PhaseSecondsRemaining);
}

void UVectorEnemyRangedAttackComponent::DrawTelegraph(const APawn* PlayerPawn) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !PlayerPawn || !World)
	{
		return;
	}
	const FVector Start = Owner->GetActorLocation() + FVector(0.0, 0.0, 50.0);
	const FVector BaseDirection =
		(PlayerPawn->GetActorLocation() - Start).GetSafeNormal2D();
	const int32 LaneCount = Pattern == EVectorEnemyRangedPattern::CorrosionVolley
		? ((AttackSequenceIndex % 2 == 0) ? 1 : 3) : 1;
	for (int32 Index = 0; Index < LaneCount; ++Index)
	{
		const double Alpha = LaneCount <= 1
			? 0.5 : static_cast<double>(Index) / (LaneCount - 1);
		const FVector Direction = BaseDirection.RotateAngleAxis(
			FMath::Lerp(-VolleySpreadDegrees, VolleySpreadDegrees, Alpha),
			FVector::UpVector).GetSafeNormal2D();
		DrawDebugDirectionalArrow(World, Start, Start + Direction * 850.0,
			55.0f, FColor::Red, false, 0.05f, 0, 5.0f);
	}
}

APawn* UVectorEnemyRangedAttackComponent::FindPlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
		Iterator; ++Iterator)
	{
		if (APlayerController* PlayerController = Iterator->Get())
		{
			if (PlayerController->GetPawn())
			{
				return PlayerController->GetPawn();
			}
		}
	}
	return nullptr;
}
