// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorLiftForkComponent.h"

#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorLiftFork, Log, All);

UVectorLiftForkComponent::UVectorLiftForkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	Timeline.ActiveSeconds = 0.12;
	Timeline.RecoverySeconds = 0.45;
}

void UVectorLiftForkComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const EVectorActionPhase PreviousPhase = Timeline.Phase;
	Timeline.Advance(DeltaTime);
	if (PreviousPhase != EVectorActionPhase::Idle && Timeline.Phase == EVectorActionPhase::Idle)
	{
		ReleaseActionLock();
	}
}

void UVectorLiftForkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAction();
	Super::EndPlay(EndPlayReason);
}

void UVectorLiftForkComponent::ActivateFork()
{
	if (Timeline.IsBusy())
	{
		UE_LOG(LogVectorLiftFork, Log, TEXT("Lift fork REJECTED: own action busy"));
		return;
	}
	AActor* Target = FVectorCombatTargeting::FindNearestMovableStableTarget(
		GetOwner(),
		FVectorCombatTargeting::ComputeCursorGroundAimDirection(GetOwner()),
		ReachCm,
		RadiusCm);
	if (!Target)
	{
		UE_LOG(LogVectorLiftFork, Log, TEXT("Lift fork: NO valid target"));
		return;
	}
	UVectorCharacterMovementComponent* Movement =
		Target->FindComponentByClass<UVectorCharacterMovementComponent>();
	UVectorStabilityComponent* Stability =
		Target->FindComponentByClass<UVectorStabilityComponent>();
	if (!Movement || !Stability)
	{
		return;
	}

	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		if (!Lock->TryAcquire(this, TEXT("LiftFork")))
		{
			UE_LOG(LogVectorLiftFork, Log, TEXT("Lift fork REJECTED: action lock=%s"),
				*Lock->GetActiveActionName().ToString());
			return;
		}
		bOwnsActionLock = true;
	}
	if (!Timeline.TryStartActive())
	{
		ReleaseActionLock();
		return;
	}

	const double AppliedStabilityDamage = Stability->ReceiveImpactHit(
		StabilityDamage, Stability->GetMassClass(), EVectorImpactType::Ground);
	const double Mass = Stability->GetEffectivePhysicalMass();
	const double VerticalSpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(
		VerticalImpulseBaseSpeedCmPerSecond, Mass);
	// Walking 会把 Z 速度投影回地面；先进入 Falling，再由统一速度队列注入垂直分量。
	Movement->SetMovementMode(MOVE_Falling);
	const bool bQueued = Movement->QueueDirectionalVelocityOverride(FVector::UpVector, VerticalSpeed);
	if (GetWorld())
	{
		DrawDebugDirectionalArrow(
			GetWorld(),
			Target->GetActorLocation(),
			Target->GetActorLocation() + FVector::UpVector * 300.0,
			20.0f,
			FColor::Yellow,
			false,
			0.45f,
			0,
			8.0f);
	}
	UE_LOG(LogVectorLiftFork, Log,
		TEXT("Lift fork hit: target=%s mass=%.2f verticalSpeed=%.0f stabilityDamage=%.1f queued=%s"),
		*Target->GetName(), Mass, VerticalSpeed, AppliedStabilityDamage,
		bQueued ? TEXT("OK") : TEXT("REJECTED"));
}

void UVectorLiftForkComponent::CancelAction()
{
	Timeline.Reset();
	ReleaseActionLock();
}

void UVectorLiftForkComponent::ReleaseActionLock()
{
	if (!bOwnsActionLock)
	{
		return;
	}
	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		Lock->Release(this);
	}
	bOwnsActionLock = false;
}
