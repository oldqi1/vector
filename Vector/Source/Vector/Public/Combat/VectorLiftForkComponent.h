// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "VectorLiftForkComponent.generated.h"

/** R 键升空叉：对前方最近目标施加按质量折算的垂直目标速度。 */
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0"))
	double StabilityDamage = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm"))
	double ReachCm = 360.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|LiftFork", meta = (ClampMin = "0.0", Units = "cm"))
	double RadiusCm = 140.0;

private:
	void ReleaseActionLock();
	bool bOwnsActionLock = false;
};
