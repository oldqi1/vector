// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorBreakableAnchorComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorStructure, Log, All);

namespace
{
	const TCHAR* SideToText(const EVectorAnchorGroupSide Side)
	{
		return Side == EVectorAnchorGroupSide::Right ? TEXT("RIGHT") : TEXT("LEFT");
	}
}

FVectorAnchorStructureResult FVectorAnchorStructureLedger::ApplyCollisionEvent(
	const double ClosingSpeedCmPerSecond,
	const double SignedLateralAlignment)
{
	FVectorAnchorStructureResult Result;
	Result.BrokenGroupCount = GetBrokenGroupCount();
	if (!FMath::IsFinite(ClosingSpeedCmPerSecond)
		|| !FMath::IsFinite(SignedLateralAlignment))
	{
		Result.Reason = TEXT("non-finite input");
		return Result;
	}
	if (ClosingSpeedCmPerSecond < FMath::Max(0.0, MinimumClosingSpeedCmPerSecond))
	{
		Result.Reason = TEXT("below closing-speed threshold");
		return Result;
	}
	if (FMath::Abs(SignedLateralAlignment)
		< FMath::Clamp(MinimumLateralAlignment, 0.0, 1.0))
	{
		Result.Reason = TEXT("impact not lateral enough");
		return Result;
	}

	Result.bAccepted = true;
	Result.Side = SignedLateralAlignment >= 0.0
		? EVectorAnchorGroupSide::Right
		: EVectorAnchorGroupSide::Left;
	bool& bSelectedBroken = Result.Side == EVectorAnchorGroupSide::Right
		? bRightBroken
		: bLeftBroken;
	if (bSelectedBroken)
	{
		Result.Reason = TEXT("contacted anchor group already broken");
		return Result;
	}

	bSelectedBroken = true;
	Result.bBrokeGroup = true;
	Result.BrokenGroupCount = GetBrokenGroupCount();
	Result.Reason = TEXT("anchor group broken");
	return Result;
}

int32 FVectorAnchorStructureLedger::GetBrokenGroupCount() const
{
	return (bLeftBroken ? 1 : 0) + (bRightBroken ? 1 : 0);
}

bool FVectorAnchorStructureLedger::IsGroupBroken(const EVectorAnchorGroupSide Side) const
{
	return Side == EVectorAnchorGroupSide::Right ? bRightBroken : bLeftBroken;
}

UVectorBreakableAnchorComponent::UVectorBreakableAnchorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorBreakableAnchorComponent::BeginPlay()
{
	Super::BeginPlay();
	Ledger.MinimumClosingSpeedCmPerSecond = MinimumClosingSpeedCmPerSecond;
	Ledger.MinimumLateralAlignment = MinimumLateralAlignment;
	ApplyPhysicalResponse();
}

void UVectorBreakableAnchorComponent::SetStructureEnabled(const bool bEnabled)
{
	bStructureEnabled = bEnabled;
	Ledger.MinimumClosingSpeedCmPerSecond = MinimumClosingSpeedCmPerSecond;
	Ledger.MinimumLateralAlignment = MinimumLateralAlignment;
	ApplyPhysicalResponse();
	UE_LOG(LogVectorStructure, Log,
		TEXT("Anchor structure configured: owner=%s enabled=%s broken=%d mass=%.2f"),
		*GetNameSafe(GetOwner()), bStructureEnabled ? TEXT("YES") : TEXT("no"),
		Ledger.GetBrokenGroupCount(),
		GetOwner() && GetOwner()->FindComponentByClass<UVectorStabilityComponent>()
			? GetOwner()->FindComponentByClass<UVectorStabilityComponent>()->GetEffectivePhysicalMass()
			: 0.0);
}

FVectorAnchorStructureResult UVectorBreakableAnchorComponent::ApplyCollisionEvent(
	const FVector& CollisionDirectionTowardOwner,
	const double ClosingSpeedCmPerSecond,
	AActor* EventSource)
{
	FVectorAnchorStructureResult Rejected;
	Rejected.BrokenGroupCount = Ledger.GetBrokenGroupCount();
	if (!bStructureEnabled || !GetOwner())
	{
		Rejected.Reason = TEXT("structure disabled");
		return Rejected;
	}
	if (!IsValid(EventSource) || EventSource == GetOwner()
		|| CollisionDirectionTowardOwner.ContainsNaN()
		|| CollisionDirectionTowardOwner.IsNearlyZero())
	{
		Rejected.Reason = TEXT("invalid finite event source or direction");
		return Rejected;
	}

	const FVector IncomingDirection = FVector::VectorPlaneProject(
		CollisionDirectionTowardOwner, FVector::UpVector).GetSafeNormal();
	const FVector OwnerRight = GetOwner()->GetActorRightVector().GetSafeNormal2D();
	const double SignedAlignment = FVector::DotProduct(IncomingDirection, OwnerRight);
	FVectorAnchorStructureResult Result = Ledger.ApplyCollisionEvent(
		ClosingSpeedCmPerSecond, SignedAlignment);

	if (Result.bBrokeGroup)
	{
		ApplyPhysicalResponse();
		OnAnchorGroupBroken.Broadcast(Result.Side, Result.BrokenGroupCount);
	}

	const UVectorStabilityComponent* Stability =
		GetOwner()->FindComponentByClass<UVectorStabilityComponent>();
	const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = CharacterOwner
		? CharacterOwner->GetCharacterMovement()
		: nullptr;
	if (Result.bBrokeGroup)
	{
		UE_LOG(LogVectorStructure, Log,
			TEXT("Structure collision: target=%s source=%s closing=%.0f lateral=%.2f side=%s result=%s broken=%d/2 launchable=%s mass=%.2f friction=%.2f braking=%.0f"),
			*GetNameSafe(GetOwner()), *GetNameSafe(EventSource), ClosingSpeedCmPerSecond,
			SignedAlignment, SideToText(Result.Side), *Result.Reason,
			Result.BrokenGroupCount, Ledger.IsLaunchable() ? TEXT("YES") : TEXT("no"),
			Stability ? Stability->GetEffectivePhysicalMass() : 0.0,
			Movement ? Movement->GroundFriction : 0.0,
			Movement ? Movement->BrakingDecelerationWalking : 0.0);
	}
	else
	{
		UE_LOG(LogVectorStructure, Verbose,
			TEXT("Structure collision: target=%s source=%s closing=%.0f lateral=%.2f side=%s result=%s broken=%d/2 launchable=%s mass=%.2f friction=%.2f braking=%.0f"),
			*GetNameSafe(GetOwner()), *GetNameSafe(EventSource), ClosingSpeedCmPerSecond,
			SignedAlignment, SideToText(Result.Side), *Result.Reason,
			Result.BrokenGroupCount, Ledger.IsLaunchable() ? TEXT("YES") : TEXT("no"),
			Stability ? Stability->GetEffectivePhysicalMass() : 0.0,
			Movement ? Movement->GroundFriction : 0.0,
			Movement ? Movement->BrakingDecelerationWalking : 0.0);
	}
	return Result;
}

void UVectorBreakableAnchorComponent::ApplyPhysicalResponse()
{
	if (!bStructureEnabled || !GetOwner())
	{
		return;
	}
	const int32 BrokenCount = Ledger.GetBrokenGroupCount();
	const double PhysicalMass = BrokenCount >= 2
		? LaunchablePhysicalMass
		: BrokenCount == 1 ? UnstablePhysicalMass : AnchoredPhysicalMass;
	const double GroundFriction = BrokenCount >= 2
		? LaunchableGroundFriction
		: BrokenCount == 1 ? UnstableGroundFriction : AnchoredGroundFriction;
	const double Braking = BrokenCount >= 2
		? LaunchableBrakingDeceleration
		: BrokenCount == 1 ? UnstableBrakingDeceleration : AnchoredBrakingDeceleration;

	if (UVectorStabilityComponent* Stability =
		GetOwner()->FindComponentByClass<UVectorStabilityComponent>())
	{
		Stability->PhysicalMassHeavy = FMath::Max(0.1, PhysicalMass);
		Stability->StaggeredPhysicalMassHeavy = FMath::Max(0.1, PhysicalMass);
	}
	if (ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = CharacterOwner->GetCharacterMovement())
		{
			Movement->GroundFriction = static_cast<float>(FMath::Max(0.0, GroundFriction));
			Movement->BrakingDecelerationWalking = static_cast<float>(FMath::Max(0.0, Braking));
		}
	}
}
