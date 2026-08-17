// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorRunProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EVectorCalibrationType : uint8
{
	Range,
	Impulse,
	Capacity,
	Recharge,
};

/** Horizontal rule modules change triggers/connections instead of adding raw damage. */
UENUM(BlueprintType)
enum class EVectorRunModuleType : uint8
{
	None,
	MomentumRecycler,
	TwinVector,
	LateralCutter,
	LiftVectorCoupler,
};

/** Pure current-run vertical progression ledger. It is deterministic and testable. */
USTRUCT(BlueprintType)
struct VECTOR_API FVectorRunCalibrationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vector|Progression")
	int32 RangeLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Vector|Progression")
	int32 ImpulseLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Vector|Progression")
	int32 CapacityLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Vector|Progression")
	int32 RechargeLevel = 0;

	bool Apply(EVectorCalibrationType Type, int32 MaximumLevel = 5);
	double GetRangeMultiplier() const;
	double GetImpulseMultiplier() const;
	double GetRechargeIntervalMultiplier() const;
	int32 GetAdditionalCells() const;
	int32 GetLevel(EVectorCalibrationType Type) const;
};

/** Pure ordering policy: the first clear changes rules before later raw calibration. */
struct VECTOR_API FVectorRunOfferPolicy
{
	static bool ShouldOfferRuleModuleFirst(
		int32 CompletedCalibrationCount,
		EVectorRunModuleType SelectedRuleModule);
};

VECTOR_API FString LexToString(EVectorCalibrationType Type);
VECTOR_API FString LexToString(EVectorRunModuleType Type);
