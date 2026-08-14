// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorLiftForkComponent.h"

#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "Combat/VectorTestDummy.h"
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
	bool bUsedCursor = false;
	const FVector AimDirection = FVectorCombatTargeting::ComputeCursorGroundAimDirection(
		GetOwner(), &bUsedCursor);
	AActor* Target = FVectorCombatTargeting::FindMostAlignedMovableStableTarget(
		GetOwner(), AimDirection,
		ReachCm,
		RadiusCm);
	if (!Target)
	{
		if (GetWorld() && GetOwner())
		{
			const FVector Start = GetOwner()->GetActorLocation();
			const FVector End = Start + AimDirection * ReachCm;
			DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.45f, 0, 6.0f);
			DrawDebugSphere(GetWorld(), End, static_cast<float>(RadiusCm), 20,
				FColor::Red, false, 0.45f, 0, 3.0f);
		}
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift fork: NO valid target aim=%s source=%s range=%.0f radius=%.0f"),
			*AimDirection.ToCompactString(), bUsedCursor ? TEXT("mouse") : TEXT("fallback"),
			ReachCm, RadiusCm);
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
	// Queue first so any movement-mode transition observes the target as impulse-driven
	// and cannot clear it through an AI/path-following stop request.
	const bool bQueued = Movement->QueueDirectionalVelocityOverride(FVector::UpVector, VerticalSpeed);
	if (!bQueued)
	{
		Timeline.Reset();
		ReleaseActionLock();
		UE_LOG(LogVectorLiftFork, Warning,
			TEXT("Lift fork REJECTED: velocity queue target=%s speed=%.0f"),
			*Target->GetName(), VerticalSpeed);
		return;
	}
	// Walking projects Z velocity onto the floor; enter Falling after the queue is armed.
	Movement->SetMovementMode(MOVE_Falling);
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(Target))
	{
		Dummy->TriggerLiftForkPresentation();
	}
	if (GetWorld())
	{
		if (GetOwner())
		{
			DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), Target->GetActorLocation(),
				FColor::Yellow, false, 0.45f, 0, 8.0f);
		}
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
		TEXT("Lift fork hit: target=%s mass=%.2f verticalSpeed=%.0f stabilityDamage=%.1f queued=%s aimSource=%s mode=%s"),
		*Target->GetName(), Mass, VerticalSpeed, AppliedStabilityDamage,
		TEXT("OK"), bUsedCursor ? TEXT("mouse") : TEXT("fallback"),
		*Movement->GetMovementName());
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
