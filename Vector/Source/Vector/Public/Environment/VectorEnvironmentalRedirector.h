// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorEnvironmentalRedirector.generated.h"

class UArrowComponent;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/** Auditable result of rotating one incoming velocity into an exit direction. */
struct VECTOR_API FVectorEnvironmentalRedirectResult
{
	bool bValid = false;
	double InputSpeedCmPerSecond = 0.0;
	double OutputSpeedCmPerSecond = 0.0;
	FVector OutputVelocity = FVector::ZeroVector;

	bool IsWithinInputBudget(double Tolerance = 1.e-6) const;
};

/** Pure math and gating shared by the runtime redirector and Automation. */
struct VECTOR_API FVectorEnvironmentalRedirectMath
{
	static FVectorEnvironmentalRedirectResult ComputeRedirect(
		const FVector& IncomingVelocity,
		const FVector& ExitDirection,
		double Efficiency);

	static bool ShouldConsume(
		bool bImpulseDriven,
		bool bAlreadyConsumedThisOverlap,
		double InputSpeedCmPerSecond,
		double MinimumInputSpeedCmPerSecond);
};

/**
 * A deterministic room converter: an impulse-driven actor entering the volume
 * keeps a lossy version of its speed, but that budget is rotated to the
 * redirector's forward direction. Walking actors are ignored, and an actor can
 * only be consumed once until it fully leaves the volume.
 */
UCLASS()
class VECTOR_API AVectorEnvironmentalRedirector : public AActor
{
	GENERATED_BODY()

public:
	AVectorEnvironmentalRedirector();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector")
	TObjectPtr<UBoxComponent> TriggerBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector")
	TObjectPtr<UStaticMeshComponent> ExitMarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector")
	TObjectPtr<UArrowComponent> ExitArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector")
	TObjectPtr<UPointLightComponent> StatusLight;

	/** Lossy speed retention. Values above one are forbidden by the solver. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double RedirectEfficiency = 0.88;

	/** Rejects residual impulse drift that is too slow to read as a route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MinimumInputSpeedCmPerSecond = 220.0;

	/** Briefly reduces ground braking so the redirected exit remains readable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector", meta = (ClampMin = "0.0", Units = "s"))
	double MomentumCarrySeconds = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Environment|Redirector|Debug")
	bool bDrawDebug = true;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void TryRedirect(AActor* OtherActor);

	TSet<TWeakObjectPtr<AActor>> ConsumedActors;
	double ActivationPulseSecondsRemaining = 0.0;
};
