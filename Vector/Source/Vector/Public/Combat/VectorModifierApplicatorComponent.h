// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "VectorModifierApplicatorComponent.generated.h"

/** 玩家调质器施加入口：Q 润滑剂，E 浮空孢子。 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorModifierApplicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorModifierApplicatorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplyLubricant();
	void ApplyBuoyantSpore();
	void CancelAction();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|ModifierApplicator")
	FVectorActionTimeline Timeline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|ModifierApplicator", meta = (ClampMin = "0.0", Units = "cm"))
	double RangeCm = 1600.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|ModifierApplicator", meta = (ClampMin = "0.0", Units = "cm"))
	double RadiusCm = 120.0;

private:
	enum class ERequestedModifier : uint8
	{
		Lubricant,
		Buoyant,
	};

	void ApplyModifier(ERequestedModifier Modifier);
	void ReleaseActionLock();

	bool bOwnsActionLock = false;
};
