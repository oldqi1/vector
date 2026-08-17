// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "VectorLiftForkComponent.generated.h"

class AActor;
class UVectorCharacterMovementComponent;

/** Equipment slot 5: redirects existing planar velocity into a vertical arc. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorLiftForkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorLiftForkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** LMB press: locks a target and lifts it, but never commits the slam. */
	void BeginForkGesture();

	/** LMB release: a deliberate drag commits one directed slam. */
	void ReleaseForkGesture();

	/** Legacy one-shot entry retained for callers outside the character input path. */
	void ActivateFork();
	void CancelAction();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork")
	FVectorActionTimeline Timeline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0"))
	double StabilityDamage = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Natural Landing", meta = (ClampMin = "0.0", Units = "cm/s"))
	double NaturalLandingMinimumFallSpeedCmPerSecond = 450.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Natural Landing", meta = (ClampMin = "0.0"))
	double NaturalLandingRadiusScale = 0.50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Natural Landing", meta = (ClampMin = "0.0"))
	double NaturalLandingDamageScale = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Natural Landing", meta = (ClampMin = "0.0"))
	double NaturalLandingMaximumDamage = 8.0;

	/** Designer/test override; normal runs unlock slam through Lift-Vector Coupler. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam")
	bool bEnableDirectedSlamWithoutModule = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm"))
	double ReachCm = 650.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm"))
	double RadiusCm = 170.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	double MaximumSlamSurfaceAngleDegrees = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam", meta = (ClampMin = "500.0", Units = "cm"))
	double SlamGroundTraceHalfHeightCm = 4000.0;

	/** Prevents a normal click from becoming an accidental slam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam", meta = (ClampMin = "0.0"))
	double MinimumSlamHoldSeconds = 0.12;

	/** Screen-space drag required before release can commit a slam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam", meta = (ClampMin = "0.0"))
	double MinimumSlamDragPixels = 28.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam|Auto", meta = (ClampMin = "0.0", Units = "cm"))
	double AutomaticSlamSearchRadiusCm = 500.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork|Slam|Auto", meta = (ClampMin = "1.0", Units = "cm"))
	double AutomaticSlamClusterRadiusCm = 260.0;

private:
	bool TryActivateDirectedSlam(
		AActor* Target,
		UVectorCharacterMovementComponent* Movement,
		const FVector& PreVelocity,
		const FVector* RequestedSurfaceOverride = nullptr,
		int32 AutomaticClusterCount = -1);
	bool IsGestureDragQualified() const;
	bool IsGestureHoldQualified() const;
	bool HasDirectedSlamUpgrade() const;
	bool FindAutomaticSlamSurface(FVector& OutSurfacePoint, int32& OutClusterCount) const;
	void DrawDirectedSlamPreview(
		const FVector* RequestedSurfaceOverride = nullptr,
		int32 AutomaticClusterCount = -1);
	void RefreshAirborneCycleState();
	void ReleaseActionLock();
	bool bOwnsActionLock = false;
	bool bForkGestureHeld = false;
	double ForkGestureHeldSeconds = 0.0;
	FVector2D ForkGestureStartCursor = FVector2D::ZeroVector;
	bool bForkGestureHasCursor = false;
	TWeakObjectPtr<AActor> AirborneFollowUpTarget;
	TSet<TWeakObjectPtr<AActor>> SlammedTargetsThisAirborneCycle;
};
