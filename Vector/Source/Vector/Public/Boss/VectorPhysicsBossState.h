// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorPhysicsBossState.generated.h"

UENUM(BlueprintType)
enum class EVectorPhysicsBossPhase : uint8
{
	AnchoredShell,
	ExposedShell,
	Overload,
	Defeated,
};

/** Tunable phase thresholds and outputs. The state machine itself owns no Actor. */
struct VECTOR_API FVectorPhysicsBossRules
{
	double ExposedHealthRatio = 0.65;
	double OverloadHealthRatio = 0.30;

	double AnchoredPhysicalMass = 8.0;
	double ExposedPhysicalMass = 4.0;
	double OverloadPhysicalMass = 3.0;

	double AnchoredRamIntervalSeconds = 3.8;
	double ExposedRamIntervalSeconds = 3.2;
	double OverloadRamIntervalSeconds = 2.5;

	double AnchoredRecoverySeconds = 0.8;
	double ExposedRecoverySeconds = 1.0;
	double OverloadRecoverySeconds = 1.3;

	int32 AnchoredMaximumAdds = 2;
	int32 ExposedMaximumAdds = 1;
	int32 OverloadMaximumAdds = 0;
};

/**
 * Deterministic, monotonic Boss phase ledger.
 *
 * Health and the first real stagger are facts supplied by gameplay components.
 * Phase changes only alter parameters; they never erase velocity, teleport the
 * Boss, clear a tether, or grant physics immunity.
 */
struct VECTOR_API FVectorPhysicsBossState
{
	explicit FVectorPhysicsBossState(
		const FVectorPhysicsBossRules& InRules = FVectorPhysicsBossRules());

	void Reset();
	bool ApplyHealthRatio(double HealthRatio);
	bool NotifyStaggered();

	EVectorPhysicsBossPhase GetPhase() const { return Phase; }
	int32 GetTransitionCount() const { return TransitionCount; }
	bool HasEverStaggered() const { return bHasEverStaggered; }
	bool IsDefeated() const { return Phase == EVectorPhysicsBossPhase::Defeated; }

	double GetEffectivePhysicalMass() const;
	double GetRamIntervalSeconds() const;
	double GetRecoverySeconds() const;
	int32 GetMaximumConcurrentAdds() const;
	bool CanSpawnAdd(int32 CurrentActiveAdds) const;
	FString Describe() const;

private:
	bool TransitionTo(EVectorPhysicsBossPhase NewPhase);
	static int32 GetPhaseRank(EVectorPhysicsBossPhase InPhase);

	FVectorPhysicsBossRules Rules;
	EVectorPhysicsBossPhase Phase = EVectorPhysicsBossPhase::AnchoredShell;
	int32 TransitionCount = 0;
	bool bHasEverStaggered = false;
};
