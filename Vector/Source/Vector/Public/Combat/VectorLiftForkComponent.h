// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "VectorLiftForkComponent.generated.h"

/** Equipment slot 5 lift fork: applies a mass-scaled vertical target speed. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorLiftForkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorLiftForkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ActivateFork();
	void CancelAction();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork")
	FVectorActionTimeline Timeline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm/s"))
	double VerticalImpulseBaseSpeedCmPerSecond = 1900.0;

	/** Heavy targets still visibly leave the ground, but remain below the 600cm/s shock threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MinimumReadableVerticalSpeedCmPerSecond = 520.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0"))
	double StabilityDamage = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm"))
	double ReachCm = 650.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm"))
	double RadiusCm = 170.0;

private:
	void ReleaseActionLock();
	bool bOwnsActionLock = false;
};
