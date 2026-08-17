// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorTrajectoryPreviewComponent.generated.h"

/** Auditable first-blocking result from a sampled ballistic capsule path. */
struct VECTOR_API FVectorTrajectoryPreviewResult
{
	bool bValid = false;
	bool bHitWorldStatic = false;
	bool bHitDynamicTarget = false;
	TWeakObjectPtr<AActor> HitDynamicActor;
	FVector ShapeCenterAtHit = FVector::ZeroVector;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	FVector PreviewEndPoint = FVector::ZeroVector;
	double HitTimeSeconds = 0.0;
	int32 SampleCount = 0;
	float ShapeRadiusCm = 0.0f;
	float ShapeHalfHeightCm = 0.0f;
};

/**
 * Shared result previewer for tool-authored ballistic motion. It samples the
 * same velocity/gravity used by execution and sweeps the moving target's real
 * capsule between adjacent samples, stopping at the first static receiver or
 * pawn. Only uncontested WorldStatic paths enter the numerical accuracy gate.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorTrajectoryPreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorTrajectoryPreviewComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool PreviewBallisticPath(
		AActor* MovingTarget,
		const FVector& StartLocation,
		const FVector& InitialVelocity,
		double GravityZCmPerSecondSquared,
		const FColor& PathColor,
		FVectorTrajectoryPreviewResult& OutResult);

	/** Freeze one valid preview and compare it with the target's next world hit. */
	bool ArmImpactVerification(
		AActor* MovingTarget,
		const FVectorTrajectoryPreviewResult& Prediction);

	static double ComputeImpactErrorCm(
		const FVector& PredictedImpactPoint,
		const FVector& ActualImpactPoint);
	static bool IsImpactWithinTolerance(
		double ErrorCm,
		double ToleranceCm);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TrajectoryPreview", meta = (ClampMin = "0.1", Units = "s"))
	double MaxPredictionSeconds = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TrajectoryPreview", meta = (ClampMin = "0.01", Units = "s"))
	double SampleStepSeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TrajectoryPreview", meta = (ClampMin = "1.0", Units = "cm"))
	double FallbackSphereRadiusCm = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TrajectoryPreview", meta = (ClampMin = "0.1"))
	double PathThickness = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TrajectoryPreview", meta = (ClampMin = "0.0", Units = "cm"))
	double VerificationToleranceCm = 120.0;

private:
	void HandleTrackedWorldStaticImpact(const FHitResult& Hit);
	void HandleTrackedDynamicInterference(const FHitResult& Hit);
	void ClearImpactVerification();

	double LastDiagnosticWorldSeconds = -1000.0;
	TWeakObjectPtr<AActor> LastDiagnosticTarget;
	TWeakObjectPtr<AActor> VerificationTarget;
	TWeakObjectPtr<class UVectorCharacterMovementComponent> VerificationMovement;
	FDelegateHandle VerificationDelegateHandle;
	FDelegateHandle VerificationInterferenceDelegateHandle;
	FVectorTrajectoryPreviewResult ArmedPrediction;
	double VerificationExpiryWorldSeconds = 0.0;
	int32 VerificationCount = 0;
	int32 VerificationPassCount = 0;
	double VerificationMaximumErrorCm = 0.0;
};
