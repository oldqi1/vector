// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/VectorTestDummy.h"
#include "VectorKineticOrb.generated.h"

class APawn;

/** Pure capped-turn helper shared by runtime guidance and Automation. */
struct VECTOR_API FVectorWeakGuidanceMath
{
	static FVector TurnDirection(
		const FVector& CurrentDirection,
		const FVector& DesiredDirection,
		double MaximumTurnRateDegreesPerSecond,
		double DeltaSeconds);
};

/** Slow, readable Boss projectile that remains a normal hammer/tether/impact target. */
UCLASS()
class VECTOR_API AVectorKineticOrb : public AVectorTestDummy
{
	GENERATED_BODY()

public:
	AVectorKineticOrb(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Makes a stationary orb into persistent player-redirectable ammunition. */
	bool Arm(AActor* SourceActor);
	bool Launch(const FVector& Direction, double SpeedCmPerSecond, AActor* SourceActor);
	bool LaunchWeakHoming(
		APawn* TargetPawn,
		double SpeedCmPerSecond,
		double GuidanceSeconds,
		double MaximumTurnRateDegreesPerSecond,
		AActor* SourceActor);
	void DisarmGuidance(const TCHAR* Reason);
	void ConfigureProjectilePresentation(
		const FLinearColor& Color,
		double LifetimeSeconds);
	bool IsGuidanceActive() const { return GuidanceSecondsRemaining > 0.0; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Orb", meta = (ClampMin = "0.1", Units = "s"))
	double MaximumLifetimeSeconds = 18.0;

private:
	UFUNCTION()
	void HandleOrbDeath();

	TWeakObjectPtr<APawn> GuidanceTarget;
	FVector GuidanceDirection = FVector::ForwardVector;
	double GuidanceSpeedCmPerSecond = 0.0;
	double GuidanceSecondsRemaining = 0.0;
	double GuidanceTurnRateDegreesPerSecond = 0.0;
	double GuidanceUpdateAccumulatorSeconds = 0.0;
};
