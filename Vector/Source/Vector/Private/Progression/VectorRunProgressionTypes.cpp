// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression/VectorRunProgressionTypes.h"

bool FVectorRunCalibrationState::Apply(
	const EVectorCalibrationType Type,
	const int32 MaximumLevel)
{
	int32* Level = nullptr;
	switch (Type)
	{
	case EVectorCalibrationType::Range: Level = &RangeLevel; break;
	case EVectorCalibrationType::Impulse: Level = &ImpulseLevel; break;
	case EVectorCalibrationType::Capacity: Level = &CapacityLevel; break;
	case EVectorCalibrationType::Recharge: Level = &RechargeLevel; break;
	default: return false;
	}
	const int32 SafeMaximum = FMath::Max(0, MaximumLevel);
	if (!Level || *Level >= SafeMaximum)
	{
		return false;
	}
	++(*Level);
	return true;
}

double FVectorRunCalibrationState::GetRangeMultiplier() const
{
	return 1.0 + 0.15 * FMath::Max(0, RangeLevel);
}

double FVectorRunCalibrationState::GetImpulseMultiplier() const
{
	return 1.0 + 0.15 * FMath::Max(0, ImpulseLevel);
}

double FVectorRunCalibrationState::GetRechargeIntervalMultiplier() const
{
	return 1.0 / (1.0 + 0.20 * FMath::Max(0, RechargeLevel));
}

int32 FVectorRunCalibrationState::GetAdditionalCells() const
{
	return FMath::Max(0, CapacityLevel);
}

int32 FVectorRunCalibrationState::GetLevel(const EVectorCalibrationType Type) const
{
	switch (Type)
	{
	case EVectorCalibrationType::Range: return RangeLevel;
	case EVectorCalibrationType::Impulse: return ImpulseLevel;
	case EVectorCalibrationType::Capacity: return CapacityLevel;
	case EVectorCalibrationType::Recharge: return RechargeLevel;
	default: return 0;
	}
}

FString LexToString(const EVectorCalibrationType Type)
{
	switch (Type)
	{
	case EVectorCalibrationType::Range: return TEXT("RANGE +15%");
	case EVectorCalibrationType::Impulse: return TEXT("IMPULSE +15%");
	case EVectorCalibrationType::Capacity: return TEXT("BATTERY +1");
	case EVectorCalibrationType::Recharge: return TEXT("RECHARGE +20%");
	default: return TEXT("UNKNOWN");
	}
}

FString LexToString(const EVectorRunModuleType Type)
{
	switch (Type)
	{
	case EVectorRunModuleType::MomentumRecycler:
		return TEXT("MOMENTUM RECYCLER");
	case EVectorRunModuleType::TwinVector:
		return TEXT("TWIN VECTOR RELAY");
	case EVectorRunModuleType::LateralCutter:
		return TEXT("LATERAL ANCHOR CUTTER");
	case EVectorRunModuleType::LiftVectorCoupler:
		return TEXT("LIFT-VECTOR COUPLER");
	case EVectorRunModuleType::None:
	default:
		return TEXT("NO MODULE");
	}
}
