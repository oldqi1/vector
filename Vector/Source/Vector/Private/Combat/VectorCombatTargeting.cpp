// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorCombatTargeting.h"

#include "Combat/VectorHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "Physics/VectorPhysicsModifierComponent.h"
#include "Stability/VectorStabilityComponent.h"

namespace
{
	template <typename PredicateType>
	AActor* FindNearest(
		const AActor* Owner,
		const FVector& Direction,
		const double RangeCm,
		const double RadiusCm,
		PredicateType&& IsValidTarget)
	{
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		if (!Owner || !World || Direction.IsNearlyZero()
			|| !FMath::IsFinite(RangeCm) || !FMath::IsFinite(RadiusCm)
			|| RangeCm <= 0.0 || RadiusCm < 0.0)
		{
			return nullptr;
		}

		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorCombatTargeting), false, Owner);
		const FVector AimDirection = Direction.GetSafeNormal2D();
		World->SweepMultiByObjectType(
			Hits,
			Owner->GetActorLocation(),
			Owner->GetActorLocation() + AimDirection * RangeCm,
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
			FCollisionShape::MakeSphere(FMath::Max(0.0, RadiusCm)),
			Params);

		AActor* BestTarget = nullptr;
		float BestDistanceSquared = MAX_FLT;
		for (const FHitResult& Hit : Hits)
		{
			AActor* Candidate = Hit.GetActor();
			if (!Candidate || Candidate == Owner || !IsValidTarget(Candidate))
			{
				continue;
			}
			if (const UVectorHealthComponent* Health =
				Candidate->FindComponentByClass<UVectorHealthComponent>(); Health && Health->IsDead())
			{
				continue;
			}
			const FVector ToCandidate = Candidate->GetActorLocation() - Owner->GetActorLocation();
			if (FVector::DotProduct(ToCandidate.GetSafeNormal2D(), AimDirection) <= 0.0)
			{
				continue;
			}

			// The Pawn sweep cannot see static walls. Verify the segment separately;
			// other Pawns intentionally do not occlude it, so crowd/yoyo targeting
			// stays usable while level geometry still blocks actions.
			if (!FVectorCombatTargeting::HasUnobstructedLine(Owner, Candidate))
			{
				continue;
			}
			const float CandidateDistanceSquared = static_cast<float>(ToCandidate.SizeSquared2D());
			if (CandidateDistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = CandidateDistanceSquared;
				BestTarget = Candidate;
			}
		}
		return BestTarget;
	}

	template <typename PredicateType>
	AActor* FindMostAligned(
		const AActor* Owner,
		const FVector& Direction,
		const double RangeCm,
		const double RadiusCm,
		PredicateType&& IsValidTarget)
	{
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		if (!Owner || !World || Direction.IsNearlyZero()
			|| !FMath::IsFinite(RangeCm) || !FMath::IsFinite(RadiusCm)
			|| RangeCm <= 0.0 || RadiusCm < 0.0)
		{
			return nullptr;
		}

		const FVector Start = Owner->GetActorLocation();
		const FVector AimDirection = Direction.GetSafeNormal2D();
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorCombatAlignedTargeting), false, Owner);
		World->SweepMultiByObjectType(
			Hits,
			Start,
			Start + AimDirection * RangeCm,
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
			FCollisionShape::MakeSphere(FMath::Max(0.0, RadiusCm)),
			Params);

		AActor* BestTarget = nullptr;
		double BestLateralDistanceSquared = TNumericLimits<double>::Max();
		double BestForwardDistance = TNumericLimits<double>::Max();
		for (const FHitResult& Hit : Hits)
		{
			AActor* Candidate = Hit.GetActor();
			if (!Candidate || Candidate == Owner || !IsValidTarget(Candidate))
			{
				continue;
			}
			if (const UVectorHealthComponent* Health =
				Candidate->FindComponentByClass<UVectorHealthComponent>(); Health && Health->IsDead())
			{
				continue;
			}

			const FVector ToCandidate = FVector::VectorPlaneProject(
				Candidate->GetActorLocation() - Start, FVector::UpVector);
			const double ForwardDistance = FVector::DotProduct(ToCandidate, AimDirection);
			if (ForwardDistance <= 0.0 || ForwardDistance > RangeCm
				|| !FVectorCombatTargeting::HasUnobstructedLine(Owner, Candidate))
			{
				continue;
			}
			const double LateralDistanceSquared = FMath::Max(
				0.0,
				static_cast<double>(ToCandidate.SizeSquared()) - FMath::Square(ForwardDistance));
			if (LateralDistanceSquared < BestLateralDistanceSquared
				|| (FMath::IsNearlyEqual(LateralDistanceSquared, BestLateralDistanceSquared, 1.0)
					&& ForwardDistance < BestForwardDistance))
			{
				BestLateralDistanceSquared = LateralDistanceSquared;
				BestForwardDistance = ForwardDistance;
				BestTarget = Candidate;
			}
		}
		return BestTarget;
	}
}

FVector FVectorCombatTargeting::ComputeHorizontalAimDirection(const AActor* Owner)
{
	if (!Owner)
	{
		return FVector::ForwardVector;
	}
	FRotator Rotation = Owner->GetActorRotation();
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			Rotation = Controller->GetControlRotation();
		}
	}
	return FRotationMatrix(FRotator(0.0, Rotation.Yaw, 0.0)).GetUnitAxis(EAxis::X).GetSafeNormal2D();
}

FVector FVectorCombatTargeting::ComputeCursorGroundAimDirection(
	const AActor* Owner,
	bool* bOutUsedCursor)
{
	if (bOutUsedCursor)
	{
		*bOutUsedCursor = false;
	}
	if (!Owner)
	{
		return FVector::ForwardVector;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	const APlayerController* PlayerController = Pawn
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	FVector CursorRayOrigin = FVector::ZeroVector;
	FVector CursorRayDirection = FVector::ZeroVector;
	if (PlayerController
		&& PlayerController->DeprojectMousePositionToWorld(
			CursorRayOrigin, CursorRayDirection)
		&& FMath::Abs(CursorRayDirection.Z) > UE_SMALL_NUMBER)
	{
		const double RayDistance =
			(Owner->GetActorLocation().Z - CursorRayOrigin.Z) / CursorRayDirection.Z;
		if (FMath::IsFinite(RayDistance) && RayDistance > 0.0)
		{
			const FVector CursorGroundPoint =
				CursorRayOrigin + CursorRayDirection * RayDistance;
			const FVector CursorDirection = FVector::VectorPlaneProject(
				CursorGroundPoint - Owner->GetActorLocation(), FVector::UpVector).GetSafeNormal();
			if (!CursorDirection.IsNearlyZero())
			{
				if (bOutUsedCursor)
				{
					*bOutUsedCursor = true;
				}
				return CursorDirection;
			}
		}
	}

	return ComputeHorizontalAimDirection(Owner);
}

bool FVectorCombatTargeting::HasUnobstructedLine(const AActor* Owner, const AActor* Target)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!Owner || !Target || !World || Target->GetWorld() != World)
	{
		return false;
	}
	FHitResult OcclusionHit;
	FCollisionQueryParams SightParams(
		SCENE_QUERY_STAT(VectorCombatTargetVisibility), false, Owner);
	return !World->LineTraceSingleByObjectType(
		OcclusionHit,
		Owner->GetActorLocation(),
		Target->GetActorLocation(),
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic)),
		SightParams)
		|| OcclusionHit.GetActor() == Target;
}

AActor* FVectorCombatTargeting::FindNearestMovableStableTarget(
	const AActor* Owner,
	const FVector& Direction,
	const double RangeCm,
	const double RadiusCm,
	const AActor* ExcludedActor)
{
	return FindNearest(Owner, Direction, RangeCm, RadiusCm, [ExcludedActor](const AActor* Candidate)
	{
		return Candidate != ExcludedActor
			&& Candidate->FindComponentByClass<UVectorCharacterMovementComponent>()
			&& Candidate->FindComponentByClass<UVectorStabilityComponent>();
	});
}

AActor* FVectorCombatTargeting::FindMostAlignedMovableStableTarget(
	const AActor* Owner,
	const FVector& Direction,
	const double RangeCm,
	const double RadiusCm,
	const AActor* ExcludedActor)
{
	return FindMostAligned(Owner, Direction, RangeCm, RadiusCm, [ExcludedActor](const AActor* Candidate)
	{
		return Candidate != ExcludedActor
			&& Candidate->FindComponentByClass<UVectorCharacterMovementComponent>()
			&& Candidate->FindComponentByClass<UVectorStabilityComponent>();
	});
}

AActor* FVectorCombatTargeting::FindNearestModifierTarget(
	const AActor* Owner,
	const FVector& Direction,
	const double RangeCm,
	const double RadiusCm)
{
	return FindNearest(Owner, Direction, RangeCm, RadiusCm, [](const AActor* Candidate)
	{
		return Candidate->FindComponentByClass<UVectorPhysicsModifierComponent>() != nullptr;
	});
}
