// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VectorPhysicsModifierMath
{
	inline double ComputeEffectiveFrictionMultiplier(
		const double EnvironmentMultiplier,
		const bool bLubricated,
		const double LubricantMultiplier)
	{
		const double SafeEnvironment = FMath::Clamp(
			FMath::IsFinite(EnvironmentMultiplier) ? EnvironmentMultiplier : 1.0, 0.0, 1.0);
		const double SafeTimed = bLubricated
			? FMath::Clamp(FMath::IsFinite(LubricantMultiplier) ? LubricantMultiplier : 1.0, 0.0, 1.0)
			: 1.0;
		return FMath::Min(SafeEnvironment, SafeTimed);
	}

	inline double ComputeEffectiveGravityScale(
		const double BaseGravityScale,
		const bool bBuoyant,
		const double BuoyantMultiplier)
	{
		if (!FMath::IsFinite(BaseGravityScale))
		{
			return 1.0;
		}
		const double Multiplier = bBuoyant
			? FMath::Clamp(FMath::IsFinite(BuoyantMultiplier) ? BuoyantMultiplier : 1.0, 0.05, 1.0)
			: 1.0;
		return FMath::Max(0.0, BaseGravityScale) * Multiplier;
	}
}
