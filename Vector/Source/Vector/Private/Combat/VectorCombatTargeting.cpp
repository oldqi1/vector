// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorCombatTargeting.h"

#include "Combat/VectorHealthComponent.h"
#include "Engine/OverlapResult.h"
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

		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorCombatTargeting), false, Owner);
		const FVector AimDirection = Direction.GetSafeNormal2D();
		World->OverlapMultiByObjectType(
			Hits,
			Owner->GetActorLocation(),
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
			FCollisionShape::MakeSphere(FMath::Max(0.0, RangeCm)),
			Params);

		AActor* BestTarget = nullptr;
		float BestDistanceSquared = MAX_FLT;
		for (const FOverlapResult& Hit : Hits)
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
			if (ToCandidate.SizeSquared() > FMath::Square(RangeCm))
			{
				continue;
			}
			const FVector PlanarToCandidate = FVector::VectorPlaneProject(
				ToCandidate, FVector::UpVector);
			const double ForwardDistance = FVector::DotProduct(PlanarToCandidate, AimDirection);
			const double LateralDistance = (PlanarToCandidate - AimDirection * ForwardDistance).Size();
			if (ForwardDistance <= 0.0 || LateralDistance > RadiusCm)
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
			const float CandidateDistanceSquared = static_cast<float>(ToCandidate.SizeSquared());
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
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorCombatAlignedTargeting), false, Owner);
		World->OverlapMultiByObjectType(
			Hits,
			Start,
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
			FCollisionShape::MakeSphere(FMath::Max(0.0, RangeCm)),
			Params);

		AActor* BestTarget = nullptr;
		double BestLateralDistanceSquared = TNumericLimits<double>::Max();
		double BestForwardDistance = TNumericLimits<double>::Max();
		for (const FOverlapResult& Hit : Hits)
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

			const FVector SpatialToCandidate = Candidate->GetActorLocation() - Start;
			if (SpatialToCandidate.SizeSquared() > FMath::Square(RangeCm))
			{
				continue;
			}
			const FVector ToCandidate = FVector::VectorPlaneProject(
				SpatialToCandidate, FVector::UpVector);
			const double ForwardDistance = FVector::DotProduct(ToCandidate, AimDirection);
			if (ForwardDistance <= 0.0 || ForwardDistance > RangeCm
				|| !FVectorCombatTargeting::HasUnobstructedLine(Owner, Candidate))
			{
				continue;
			}
			const double LateralDistanceSquared = FMath::Max(
				0.0,
				static_cast<double>(ToCandidate.SizeSquared()) - FMath::Square(ForwardDistance));
			if (LateralDistanceSquared > FMath::Square(RadiusCm))
			{
				continue;
			}
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

bool FVectorCombatTargeting::ComputeCursorWorldStaticPoint(
	const AActor* Owner,
	FVector& OutImpactPoint,
	FVector& OutImpactNormal)
{
	OutImpactPoint = FVector::ZeroVector;
	OutImpactNormal = FVector::UpVector;
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const APawn* Pawn = Cast<APawn>(Owner);
	const APlayerController* PlayerController = Pawn
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Owner || !World || !PlayerController)
	{
		return false;
	}

	FVector CursorRayOrigin = FVector::ZeroVector;
	FVector CursorRayDirection = FVector::ZeroVector;
	if (!PlayerController->DeprojectMousePositionToWorld(
		CursorRayOrigin, CursorRayDirection)
		|| CursorRayDirection.IsNearlyZero())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(VectorCursorWorldStaticPoint), false, Owner);
	const bool bHit = World->LineTraceSingleByObjectType(
		Hit,
		CursorRayOrigin,
		CursorRayOrigin + CursorRayDirection.GetSafeNormal() * 100000.0,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic)),
		Params);
	if (!bHit || !Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN()
		|| Hit.ImpactNormal.ContainsNaN())
	{
		return false;
	}
	OutImpactPoint = Hit.ImpactPoint;
	OutImpactNormal = Hit.ImpactNormal.GetSafeNormal();
	return !OutImpactNormal.IsNearlyZero();
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

bool FVectorCombatTargeting::IsCursorCandidatePreferred(
	const FVectorCursorTargetScore& Candidate,
	const FVectorCursorTargetScore& CurrentBest,
	const double AirborneOverlapTolerancePixels)
{
	const double CandidateScreenDistance = FMath::Sqrt(FMath::Max(
		0.0, Candidate.ScreenDistanceSquared));
	const double BestScreenDistance = FMath::Sqrt(FMath::Max(
		0.0, CurrentBest.ScreenDistanceSquared));
	const double OverlapTolerance = FMath::Max(0.0, AirborneOverlapTolerancePixels);
	if (CandidateScreenDistance + OverlapTolerance < BestScreenDistance)
	{
		return true;
	}
	if (BestScreenDistance + OverlapTolerance < CandidateScreenDistance)
	{
		return false;
	}
	if (Candidate.bAirborne != CurrentBest.bAirborne)
	{
		return Candidate.bAirborne;
	}
	if (!FMath::IsNearlyEqual(
		Candidate.ScreenDistanceSquared, CurrentBest.ScreenDistanceSquared, 1.0))
	{
		return Candidate.ScreenDistanceSquared < CurrentBest.ScreenDistanceSquared;
	}
	if (!FMath::IsNearlyEqual(
		Candidate.SpatialDistanceSquared, CurrentBest.SpatialDistanceSquared, 1.0))
	{
		return Candidate.SpatialDistanceSquared < CurrentBest.SpatialDistanceSquared;
	}
	return CurrentBest.StableName.IsEmpty()
		|| Candidate.StableName.Compare(CurrentBest.StableName) < 0;
}

AActor* FVectorCombatTargeting::FindBestCursorMovableStableTarget(
	const AActor* Owner,
	const FVector& FallbackDirection,
	const double RangeCm,
	const double FallbackRadiusCm,
	const double ScreenRadiusPixels,
	const AActor* ExcludedActor)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const APawn* Pawn = Cast<APawn>(Owner);
	const APlayerController* PlayerController = Pawn
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	float CursorX = 0.0f;
	float CursorY = 0.0f;
	if (!Owner || !World || !PlayerController
		|| !PlayerController->GetMousePosition(CursorX, CursorY)
		|| !FMath::IsFinite(RangeCm) || RangeCm <= 0.0
		|| !FMath::IsFinite(ScreenRadiusPixels) || ScreenRadiusPixels < 0.0)
	{
		return FindMostAlignedMovableStableTarget(
			Owner, FallbackDirection, RangeCm, FallbackRadiusCm, ExcludedActor);
	}

	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorCombatCursorTargeting), false, Owner);
	World->OverlapMultiByObjectType(
		Hits,
		Owner->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
		FCollisionShape::MakeSphere(RangeCm),
		Params);

	AActor* BestTarget = nullptr;
	FVectorCursorTargetScore BestScore;
	for (const FOverlapResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!Candidate || Candidate == Owner || Candidate == ExcludedActor
			|| !Candidate->FindComponentByClass<UVectorCharacterMovementComponent>()
			|| !Candidate->FindComponentByClass<UVectorStabilityComponent>())
		{
			continue;
		}
		if (const UVectorHealthComponent* Health =
			Candidate->FindComponentByClass<UVectorHealthComponent>(); Health && Health->IsDead())
		{
			continue;
		}
		const FVector SpatialDelta = Candidate->GetActorLocation() - Owner->GetActorLocation();
		if (SpatialDelta.SizeSquared() > FMath::Square(RangeCm)
			|| !HasUnobstructedLine(Owner, Candidate))
		{
			continue;
		}

		const FVector BoundsOrigin = Candidate->GetComponentsBoundingBox().GetCenter();
		FVector2D CandidateScreenPosition = FVector2D::ZeroVector;
		if (!PlayerController->ProjectWorldLocationToScreen(
			BoundsOrigin, CandidateScreenPosition, false))
		{
			continue;
		}
		const double ScreenDistanceSquared = FVector2D::DistSquared(
			CandidateScreenPosition, FVector2D(CursorX, CursorY));
		if (ScreenDistanceSquared > FMath::Square(ScreenRadiusPixels))
		{
			continue;
		}

		const UVectorCharacterMovementComponent* Movement =
			Candidate->FindComponentByClass<UVectorCharacterMovementComponent>();
		const FVector EffectiveVelocity = Movement
			? Movement->GetEffectiveVelocityForPendingStep() : Candidate->GetVelocity();
		FVectorCursorTargetScore CandidateScore;
		CandidateScore.ScreenDistanceSquared = ScreenDistanceSquared;
		CandidateScore.bAirborne = Movement
			&& (Movement->IsFalling()
				|| FMath::Abs(EffectiveVelocity.Z) > UE_KINDA_SMALL_NUMBER);
		CandidateScore.SpatialDistanceSquared = SpatialDelta.SizeSquared();
		CandidateScore.StableName = Candidate->GetName();
		if (!BestTarget || IsCursorCandidatePreferred(CandidateScore, BestScore))
		{
			BestTarget = Candidate;
			BestScore = MoveTemp(CandidateScore);
		}
	}
	return BestTarget;
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
