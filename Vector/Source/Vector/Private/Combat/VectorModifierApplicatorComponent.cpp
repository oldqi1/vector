// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorModifierApplicatorComponent.h"

#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Physics/VectorPhysicsModifierComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorModifierWeapon, Log, All);

UVectorModifierApplicatorComponent::UVectorModifierApplicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	Timeline.ActiveSeconds = 0.10;
	Timeline.RecoverySeconds = 0.35;
}

void UVectorModifierApplicatorComponent::TickComponent(
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

void UVectorModifierApplicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAction();
	Super::EndPlay(EndPlayReason);
}

void UVectorModifierApplicatorComponent::ApplyLubricant()
{
	ApplyModifier(ERequestedModifier::Lubricant);
}

void UVectorModifierApplicatorComponent::ApplyBuoyantSpore()
{
	ApplyModifier(ERequestedModifier::Buoyant);
}

void UVectorModifierApplicatorComponent::CancelAction()
{
	Timeline.Reset();
	ReleaseActionLock();
}

void UVectorModifierApplicatorComponent::ApplyModifier(const ERequestedModifier Modifier)
{
	const TCHAR* ModifierLabel = Modifier == ERequestedModifier::Lubricant
		? TEXT("Lubricant") : TEXT("BuoyantSpore");
	if (Timeline.IsBusy())
	{
		UE_LOG(LogVectorModifierWeapon, Log, TEXT("%s REJECTED: applicator busy"), ModifierLabel);
		return;
	}

	AActor* Target = FVectorCombatTargeting::FindNearestModifierTarget(
		GetOwner(),
		FVectorCombatTargeting::ComputeCursorGroundAimDirection(GetOwner()),
		RangeCm,
		RadiusCm);
	if (!Target)
	{
		UE_LOG(LogVectorModifierWeapon, Log, TEXT("%s: NO valid target in %.0fcm"),
			ModifierLabel, RangeCm);
		return;
	}
	UVectorPhysicsModifierComponent* TargetModifier =
		Target->FindComponentByClass<UVectorPhysicsModifierComponent>();
	if (!TargetModifier)
	{
		return;
	}

	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		if (!Lock->TryAcquire(this, ModifierLabel))
		{
			UE_LOG(LogVectorModifierWeapon, Log, TEXT("%s REJECTED: action lock=%s"),
				ModifierLabel, *Lock->GetActiveActionName().ToString());
			return;
		}
		bOwnsActionLock = true;
	}
	if (!Timeline.TryStartActive())
	{
		ReleaseActionLock();
		return;
	}

	if (Modifier == ERequestedModifier::Lubricant)
	{
		TargetModifier->ApplyLubricant();
	}
	else
	{
		TargetModifier->ApplyBuoyantSpore();
	}
	if (GetWorld() && GetOwner())
	{
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), Target->GetActorLocation(),
			Modifier == ERequestedModifier::Lubricant ? FColor::Blue : FColor::Cyan,
			false, 0.35f, 0, 8.0f);
	}
	UE_LOG(LogVectorModifierWeapon, Log, TEXT("%s applied to %s"), ModifierLabel, *Target->GetName());
}

void UVectorModifierApplicatorComponent::ReleaseActionLock()
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
