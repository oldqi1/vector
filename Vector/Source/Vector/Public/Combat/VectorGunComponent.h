// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "VectorGunComponent.generated.h"

/** Instant primary weapon: writes a cursor-directed velocity into one physical target. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorGunComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool Fire();
	void CancelAction();

	int32 GetCurrentCells() const { return CurrentCells; }
	int32 GetMaximumCells() const;
	double GetEffectiveRangeCm() const;
	double GetEffectiveImpulseBudget() const;
	double GetRechargeProgress() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.0", Units = "cm"))
	double BaseRangeCm = 900.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.0", Units = "cm"))
	double TargetingRadiusCm = 190.0;

	/** Cursor acquisition radius used by the fixed-camera result preview. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun|Preview", meta = (ClampMin = "0.0"))
	double TargetingScreenRadiusPixels = 110.0;

	/** Momentum-like budget: target speed = budget / effective physical mass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.0"))
	double BaseImpulseBudget = 1800.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.0"))
	double BaseHealthDamage = 12.0;

	/** Direct-fire fallback remains possible, but cannot efficiently skip a structure contract. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double AnchoredStructureHealthDamageMultiplier = 0.20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double UnstableStructureHealthDamageMultiplier = 0.50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.0"))
	double BaseStabilityDamage = 16.0;

	/** Reward for the explicit 5 -> 1 tool sentence; multiplies impulse, not raw damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun|Combo", meta = (ClampMin = "1.0"))
	double LiftForkComboImpulseMultiplier = 1.25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "1"))
	int32 BaseMaximumCells = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.05", Units = "s"))
	double BaseRechargeSecondsPerCell = 0.85;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun", meta = (ClampMin = "0.0", Units = "cm/s"))
	double RecoilVelocityChangeCmPerSecond = 85.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun|Modules", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MomentumRecyclerThresholdCmPerSecond = 600.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Gun|Modules", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double TwinVectorImpulseFraction = 0.45;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Gun")
	FVectorActionTimeline Timeline;

private:
	class UVectorRunProgressionComponent* FindProgression() const;
	AActor* FindCursorTarget(const FVector& Direction, double RangeCm) const;
	void UpdateAimPreview();
	void ReleaseActionLock();

	int32 CurrentCells = 0;
	double RechargeElapsedSeconds = 0.0;
	bool bOwnsActionLock = false;
	TWeakObjectPtr<AActor> PreviewTarget;
	double PreviewLogCooldownSecondsRemaining = 0.0;
};
