// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorTrajectoryPreviewComponent.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorTrajectoryPreview, Log, All);

UVectorTrajectoryPreviewComponent::UVectorTrajectoryPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorTrajectoryPreviewComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearImpactVerification();
	Super::EndPlay(EndPlayReason);
}

bool UVectorTrajectoryPreviewComponent::PreviewBallisticPath(
	AActor* MovingTarget,
	const FVector& StartLocation,
	const FVector& InitialVelocity,
	const double GravityZCmPerSecondSquared,
	const FColor& PathColor,
	FVectorTrajectoryPreviewResult& OutResult)
{
	OutResult = FVectorTrajectoryPreviewResult();
	UWorld* World = GetWorld();
	if (!World || !MovingTarget
		|| StartLocation.ContainsNaN() || InitialVelocity.ContainsNaN()
		|| !FMath::IsFinite(GravityZCmPerSecondSquared)
		|| GravityZCmPerSecondSquared >= 0.0
		|| !FMath::IsFinite(MaxPredictionSeconds)
		|| !FMath::IsFinite(SampleStepSeconds)
		|| MaxPredictionSeconds <= 0.0 || SampleStepSeconds <= 0.0)
	{
		return false;
	}

	float ShapeRadius = static_cast<float>(FMath::Max(1.0, FallbackSphereRadiusCm));
	float ShapeHalfHeight = ShapeRadius;
	FCollisionShape Shape = FCollisionShape::MakeSphere(ShapeRadius);
	if (const ACharacter* Character = Cast<ACharacter>(MovingTarget))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			ShapeRadius = Capsule->GetScaledCapsuleRadius();
			ShapeHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			Shape = FCollisionShape::MakeCapsule(ShapeRadius, ShapeHalfHeight);
		}
	}
	OutResult.bValid = true;
	OutResult.ShapeRadiusCm = ShapeRadius;
	OutResult.ShapeHalfHeightCm = ShapeHalfHeight;

	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(VectorTrajectoryFirstWorldBlock), false, MovingTarget);
	Params.AddIgnoredActor(GetOwner());
	FCollisionObjectQueryParams ObjectParams(
		ECC_TO_BITFIELD(ECC_WorldStatic));
	// Pawns are physical pieces in this game. Showing an arc through a pawn and
	// only predicting the wall behind it would be mechanically misleading.
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	const double SafeStep = FMath::Clamp(SampleStepSeconds, 0.01, MaxPredictionSeconds);
	const int32 MaximumSamples = FMath::Max(
		1, FMath::CeilToInt(MaxPredictionSeconds / SafeStep));
	FVector PreviousPoint = StartLocation;
	for (int32 SampleIndex = 1; SampleIndex <= MaximumSamples; ++SampleIndex)
	{
		const double TimeSeconds = FMath::Min(
			MaxPredictionSeconds, SampleIndex * SafeStep);
		const FVector NextPoint = FVectorImpactMath::SampleBallisticPosition(
			StartLocation, InitialVelocity, GravityZCmPerSecondSquared, TimeSeconds);
		FHitResult Hit;
		const bool bHit = World->SweepSingleByObjectType(
			Hit, PreviousPoint, NextPoint, FQuat::Identity,
			ObjectParams, Shape, Params);
		++OutResult.SampleCount;
		const FVector SegmentDelta = NextPoint - PreviousPoint;
		const bool bMovingIntoSurface = bHit
			&& Hit.Component.IsValid()
			&& !Hit.bStartPenetrating
			&& FVector::DotProduct(SegmentDelta, Hit.ImpactNormal) < 0.0;
		if (bMovingIntoSurface)
		{
			const ECollisionChannel HitObjectType =
				Hit.Component->GetCollisionObjectType();
			OutResult.bHitWorldStatic = HitObjectType == ECC_WorldStatic;
			OutResult.bHitDynamicTarget = HitObjectType == ECC_Pawn;
			OutResult.HitDynamicActor = OutResult.bHitDynamicTarget
				? Hit.GetActor() : nullptr;
			OutResult.ShapeCenterAtHit = Hit.Location;
			OutResult.ImpactPoint = Hit.ImpactPoint;
			OutResult.ImpactNormal = Hit.ImpactNormal;
			OutResult.PreviewEndPoint = Hit.Location;
			OutResult.HitTimeSeconds = TimeSeconds;
			DrawDebugLine(World, PreviousPoint, Hit.Location,
				PathColor, false, 0.05f, 0, static_cast<float>(PathThickness));
			DrawDebugSphere(World, Hit.ImpactPoint, 28.0f, 16,
				OutResult.bHitWorldStatic ? FColor::Green : FColor::Yellow,
				false, 0.05f, 0, 4.0f);
			break;
		}
		DrawDebugLine(World, PreviousPoint, NextPoint,
			PathColor, false, 0.05f, 0, static_cast<float>(PathThickness));
		PreviousPoint = NextPoint;
		OutResult.PreviewEndPoint = NextPoint;
		if (TimeSeconds >= MaxPredictionSeconds)
		{
			break;
		}
	}
	if (!OutResult.bHitWorldStatic && !OutResult.bHitDynamicTarget)
	{
		DrawDebugSphere(World, OutResult.PreviewEndPoint, 28.0f, 16,
			FColor::Red, false, 0.05f, 0, 4.0f);
	}

	const double WorldSeconds = World->GetTimeSeconds();
	if (MovingTarget != LastDiagnosticTarget.Get()
		|| WorldSeconds - LastDiagnosticWorldSeconds >= 0.5)
	{
		const TCHAR* HitType = OutResult.bHitWorldStatic
			? TEXT("WORLD_STATIC")
			: (OutResult.bHitDynamicTarget ? TEXT("DYNAMIC_TARGET") : TEXT("NONE"));
		UE_LOG(LogVectorTrajectoryPreview, Log,
			TEXT("Trajectory preview: target=%s velocity=%s gravity=%.0f shape=(r=%.0f,h=%.0f) samples=%d hit=%s hitActor=%s hitTime=%.2f impact=%s check=PASS"),
			*MovingTarget->GetName(), *InitialVelocity.ToCompactString(),
			GravityZCmPerSecondSquared, ShapeRadius, ShapeHalfHeight,
			OutResult.SampleCount, HitType,
			*GetNameSafe(OutResult.HitDynamicActor.Get()),
			OutResult.HitTimeSeconds, *OutResult.ImpactPoint.ToCompactString());
		LastDiagnosticTarget = MovingTarget;
		LastDiagnosticWorldSeconds = WorldSeconds;
	}
	return true;
}

bool UVectorTrajectoryPreviewComponent::ArmImpactVerification(
	AActor* MovingTarget,
	const FVectorTrajectoryPreviewResult& Prediction)
{
	ClearImpactVerification();
	UWorld* World = GetWorld();
	UVectorCharacterMovementComponent* Movement = MovingTarget
		? MovingTarget->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	if (!World || !MovingTarget || !Movement
		|| !Prediction.bValid || !Prediction.bHitWorldStatic)
	{
		return false;
	}
	VerificationTarget = MovingTarget;
	VerificationMovement = Movement;
	ArmedPrediction = Prediction;
	VerificationExpiryWorldSeconds = World->GetTimeSeconds()
		+ FMath::Max(MaxPredictionSeconds + 0.5, Prediction.HitTimeSeconds + 1.0);
	VerificationDelegateHandle = Movement->OnWorldStaticImpact.AddUObject(
		this, &UVectorTrajectoryPreviewComponent::HandleTrackedWorldStaticImpact);
	VerificationInterferenceDelegateHandle = Movement->OnDynamicInterference.AddUObject(
		this, &UVectorTrajectoryPreviewComponent::HandleTrackedDynamicInterference);
	UE_LOG(LogVectorTrajectoryPreview, Log,
		TEXT("Trajectory verification armed: target=%s predicted=%s hitTime=%.2f tolerance=%.0f check=PASS"),
		*MovingTarget->GetName(), *Prediction.ImpactPoint.ToCompactString(),
		Prediction.HitTimeSeconds, VerificationToleranceCm);
	return true;
}

double UVectorTrajectoryPreviewComponent::ComputeImpactErrorCm(
	const FVector& PredictedImpactPoint,
	const FVector& ActualImpactPoint)
{
	if (PredictedImpactPoint.ContainsNaN() || ActualImpactPoint.ContainsNaN()
		|| !FMath::IsFinite(PredictedImpactPoint.X)
		|| !FMath::IsFinite(PredictedImpactPoint.Y)
		|| !FMath::IsFinite(PredictedImpactPoint.Z)
		|| !FMath::IsFinite(ActualImpactPoint.X)
		|| !FMath::IsFinite(ActualImpactPoint.Y)
		|| !FMath::IsFinite(ActualImpactPoint.Z))
	{
		return TNumericLimits<double>::Max();
	}
	return FVector::Distance(PredictedImpactPoint, ActualImpactPoint);
}

bool UVectorTrajectoryPreviewComponent::IsImpactWithinTolerance(
	const double ErrorCm,
	const double ToleranceCm)
{
	return FMath::IsFinite(ErrorCm)
		&& FMath::IsFinite(ToleranceCm)
		&& ErrorCm >= 0.0
		&& ErrorCm <= FMath::Max(0.0, ToleranceCm);
}

void UVectorTrajectoryPreviewComponent::HandleTrackedWorldStaticImpact(
	const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	AActor* Target = VerificationTarget.Get();
	if (!World || !Target)
	{
		ClearImpactVerification();
		return;
	}
	if (World->GetTimeSeconds() > VerificationExpiryWorldSeconds)
	{
		UE_LOG(LogVectorTrajectoryPreview, Warning,
			TEXT("Trajectory verification expired: target=%s predicted=%s check=STALE"),
			*Target->GetName(), *ArmedPrediction.ImpactPoint.ToCompactString());
		ClearImpactVerification();
		return;
	}

	const FVector ActualImpactPoint = Hit.ImpactPoint.IsNearlyZero()
		? Hit.Location : Hit.ImpactPoint;
	const double ErrorCm = ComputeImpactErrorCm(
		ArmedPrediction.ImpactPoint, ActualImpactPoint);
	const double ToleranceCm = FMath::Max(0.0, VerificationToleranceCm);
	const bool bPassed = IsImpactWithinTolerance(ErrorCm, ToleranceCm);
	++VerificationCount;
	VerificationPassCount += bPassed ? 1 : 0;
	VerificationMaximumErrorCm = FMath::Max(VerificationMaximumErrorCm, ErrorCm);
	DrawDebugLine(World, ArmedPrediction.ImpactPoint, ActualImpactPoint,
		bPassed ? FColor::Green : FColor::Red, false, 2.0f, 0, 6.0f);
	DrawDebugSphere(World, ActualImpactPoint, 36.0f, 20,
		bPassed ? FColor::Green : FColor::Red, false, 2.0f, 0, 5.0f);
	UE_LOG(LogVectorTrajectoryPreview, Log,
		TEXT("Trajectory verification: target=%s predicted=%s actual=%s error=%.1f tolerance=%.1f samples=%d passed=%d maxError=%.1f check=%s"),
		*Target->GetName(), *ArmedPrediction.ImpactPoint.ToCompactString(),
		*ActualImpactPoint.ToCompactString(), ErrorCm, ToleranceCm,
		VerificationCount, VerificationPassCount, VerificationMaximumErrorCm,
		bPassed ? TEXT("PASS") : TEXT("FAIL"));
	if (VerificationCount > 0 && VerificationCount % 20 == 0)
	{
		const bool bGatePassed = VerificationPassCount == VerificationCount
			&& VerificationMaximumErrorCm <= ToleranceCm;
		UE_LOG(LogVectorTrajectoryPreview, Log,
			TEXT("Trajectory verification gate: samples=%d passed=%d maxError=%.1f tolerance=%.1f check=%s"),
			VerificationCount, VerificationPassCount, VerificationMaximumErrorCm,
			ToleranceCm, bGatePassed ? TEXT("PASS") : TEXT("FAIL"));

		// Treat each set of 20 impacts as an independent acceptance batch. A
		// tuning failure in an older build must not poison every later gate.
		VerificationCount = 0;
		VerificationPassCount = 0;
		VerificationMaximumErrorCm = 0.0;
	}
	ClearImpactVerification();
}

void UVectorTrajectoryPreviewComponent::HandleTrackedDynamicInterference(
	const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	AActor* Target = VerificationTarget.Get();
	if (!World || !Target)
	{
		ClearImpactVerification();
		return;
	}

	const FVector InterferencePoint = Hit.ImpactPoint.IsNearlyZero()
		? Hit.Location : Hit.ImpactPoint;
	DrawDebugSphere(World, InterferencePoint, 34.0f, 16,
		FColor::Yellow, false, 1.5f, 0, 4.0f);
	UE_LOG(LogVectorTrajectoryPreview, Log,
		TEXT("Trajectory verification invalidated: target=%s interferer=%s point=%s reason=DYNAMIC_INTERFERENCE samples=%d passed=%d check=SKIPPED"),
		*Target->GetName(), *GetNameSafe(Hit.GetActor()),
		*InterferencePoint.ToCompactString(), VerificationCount,
		VerificationPassCount);
	ClearImpactVerification();
}

void UVectorTrajectoryPreviewComponent::ClearImpactVerification()
{
	if (UVectorCharacterMovementComponent* Movement = VerificationMovement.Get();
		Movement)
	{
		if (VerificationDelegateHandle.IsValid())
		{
			Movement->OnWorldStaticImpact.Remove(VerificationDelegateHandle);
		}
		if (VerificationInterferenceDelegateHandle.IsValid())
		{
			Movement->OnDynamicInterference.Remove(
				VerificationInterferenceDelegateHandle);
		}
	}
	VerificationTarget.Reset();
	VerificationMovement.Reset();
	VerificationDelegateHandle.Reset();
	VerificationInterferenceDelegateHandle.Reset();
	ArmedPrediction = FVectorTrajectoryPreviewResult();
	VerificationExpiryWorldSeconds = 0.0;
}
