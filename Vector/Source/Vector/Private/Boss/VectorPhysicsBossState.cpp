// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boss/VectorPhysicsBossState.h"

namespace VectorPhysicsBossStateInternal
{
	FString PhaseToString(const EVectorPhysicsBossPhase Phase)
	{
		switch (Phase)
		{
		case EVectorPhysicsBossPhase::AnchoredShell: return TEXT("ANCHORED");
		case EVectorPhysicsBossPhase::ExposedShell: return TEXT("EXPOSED");
		case EVectorPhysicsBossPhase::Overload: return TEXT("OVERLOAD");
		case EVectorPhysicsBossPhase::Defeated: return TEXT("DEFEATED");
		default: return TEXT("UNKNOWN");
		}
	}
}

FVectorPhysicsBossState::FVectorPhysicsBossState(const FVectorPhysicsBossRules& InRules)
	: Rules(InRules)
{
	Reset();
}

void FVectorPhysicsBossState::Reset()
{
	Phase = EVectorPhysicsBossPhase::AnchoredShell;
	TransitionCount = 0;
	bHasEverStaggered = false;
	BrokenStructureGroupCount = 0;
	StaggerResolveSecondsRemaining = 0.0;
}

bool FVectorPhysicsBossState::ApplyHealthRatio(const double HealthRatio)
{
	if (!FMath::IsFinite(HealthRatio))
	{
		return false;
	}

	if (FMath::Clamp(HealthRatio, 0.0, 1.0) <= 0.0)
	{
		return TransitionTo(EVectorPhysicsBossPhase::Defeated);
	}
	return false;
}

bool FVectorPhysicsBossState::NotifyStructureBroken(const int32 BrokenGroupCount)
{
	if (IsDefeated())
	{
		return false;
	}
	BrokenStructureGroupCount = FMath::Max(BrokenStructureGroupCount, BrokenGroupCount);
	if (BrokenStructureGroupCount >= 2)
	{
		return TransitionTo(EVectorPhysicsBossPhase::Overload);
	}
	if (BrokenStructureGroupCount >= 1)
	{
		return TransitionTo(EVectorPhysicsBossPhase::ExposedShell);
	}
	return false;
}

bool FVectorPhysicsBossState::NotifyStaggered()
{
	bHasEverStaggered = true;
	return false;
}

bool FVectorPhysicsBossState::TryBeginStaggerResolve(const double ResolveDurationSeconds)
{
	bHasEverStaggered = true;
	if (IsDefeated() || IsStaggerResolveActive()
		|| !FMath::IsFinite(ResolveDurationSeconds) || ResolveDurationSeconds <= 0.0)
	{
		return false;
	}
	StaggerResolveSecondsRemaining = ResolveDurationSeconds;
	return true;
}

void FVectorPhysicsBossState::AdvanceStaggerResolve(const double DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0
		|| StaggerResolveSecondsRemaining <= 0.0)
	{
		return;
	}
	StaggerResolveSecondsRemaining = FMath::Max(
		0.0, StaggerResolveSecondsRemaining - DeltaSeconds);
}

double FVectorPhysicsBossState::GetEffectivePhysicalMass() const
{
	switch (Phase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell:
		return FMath::Max(0.1, Rules.AnchoredPhysicalMass);
	case EVectorPhysicsBossPhase::ExposedShell:
		return FMath::Max(0.1, Rules.ExposedPhysicalMass);
	case EVectorPhysicsBossPhase::Overload:
		return FMath::Max(0.1, Rules.OverloadPhysicalMass);
	case EVectorPhysicsBossPhase::Defeated:
	default:
		return FMath::Max(0.1, Rules.OverloadPhysicalMass);
	}
}

double FVectorPhysicsBossState::GetPursuitSpeedCmPerSecond() const
{
	switch (Phase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell:
		return FMath::Max(0.0, Rules.AnchoredPursuitSpeedCmPerSecond);
	case EVectorPhysicsBossPhase::ExposedShell:
		return FMath::Max(0.0, Rules.ExposedPursuitSpeedCmPerSecond);
	case EVectorPhysicsBossPhase::Overload:
		return FMath::Max(0.0, Rules.OverloadPursuitSpeedCmPerSecond);
	case EVectorPhysicsBossPhase::Defeated:
	default:
		return 0.0;
	}
}

double FVectorPhysicsBossState::GetRamIntervalSeconds() const
{
	switch (Phase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell:
		return FMath::Max(0.0, Rules.AnchoredRamIntervalSeconds);
	case EVectorPhysicsBossPhase::ExposedShell:
		return FMath::Max(0.0, Rules.ExposedRamIntervalSeconds);
	case EVectorPhysicsBossPhase::Overload:
		return FMath::Max(0.0, Rules.OverloadRamIntervalSeconds);
	case EVectorPhysicsBossPhase::Defeated:
	default:
		return 0.0;
	}
}

double FVectorPhysicsBossState::GetRecoverySeconds() const
{
	switch (Phase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell:
		return FMath::Max(0.0, Rules.AnchoredRecoverySeconds);
	case EVectorPhysicsBossPhase::ExposedShell:
		return FMath::Max(0.0, Rules.ExposedRecoverySeconds);
	case EVectorPhysicsBossPhase::Overload:
		return FMath::Max(0.0, Rules.OverloadRecoverySeconds);
	case EVectorPhysicsBossPhase::Defeated:
	default:
		return 0.0;
	}
}

int32 FVectorPhysicsBossState::GetMaximumConcurrentAdds() const
{
	switch (Phase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell:
		return FMath::Max(0, Rules.AnchoredMaximumAdds);
	case EVectorPhysicsBossPhase::ExposedShell:
		return FMath::Max(0, Rules.ExposedMaximumAdds);
	case EVectorPhysicsBossPhase::Overload:
	case EVectorPhysicsBossPhase::Defeated:
	default:
		return FMath::Max(0, Rules.OverloadMaximumAdds);
	}
}

bool FVectorPhysicsBossState::CanSpawnAdd(const int32 CurrentActiveAdds) const
{
	return !IsDefeated()
		&& FMath::Max(0, CurrentActiveAdds) < GetMaximumConcurrentAdds();
}

EVectorPhysicsBossAttack FVectorPhysicsBossState::SelectAttack(
	const int32 SequenceIndex,
	const bool bAmmoAvailable) const
{
	const int32 SafeIndex = FMath::Max(0, SequenceIndex);
	switch (Phase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell:
	{
		constexpr EVectorPhysicsBossAttack Pattern[] =
		{
			EVectorPhysicsBossAttack::AmmoLaunch,
			EVectorPhysicsBossAttack::Ram,
		};
		const EVectorPhysicsBossAttack Selected = Pattern[SafeIndex % UE_ARRAY_COUNT(Pattern)];
		return Selected == EVectorPhysicsBossAttack::AmmoLaunch && !bAmmoAvailable
			? EVectorPhysicsBossAttack::Ram : Selected;
	}
	case EVectorPhysicsBossPhase::ExposedShell:
	{
		constexpr EVectorPhysicsBossAttack Pattern[] =
		{
			EVectorPhysicsBossAttack::AmmoLaunch,
			EVectorPhysicsBossAttack::Ram,
			EVectorPhysicsBossAttack::Slam,
		};
		const EVectorPhysicsBossAttack Selected = Pattern[SafeIndex % UE_ARRAY_COUNT(Pattern)];
		return Selected == EVectorPhysicsBossAttack::AmmoLaunch && !bAmmoAvailable
			? EVectorPhysicsBossAttack::AerialBurst : Selected;
	}
	case EVectorPhysicsBossPhase::Overload:
	{
		constexpr EVectorPhysicsBossAttack Pattern[] =
		{
			EVectorPhysicsBossAttack::AmmoLaunch,
			EVectorPhysicsBossAttack::Ram,
			EVectorPhysicsBossAttack::Slam,
			EVectorPhysicsBossAttack::AerialBurst,
		};
		const EVectorPhysicsBossAttack Selected = Pattern[SafeIndex % UE_ARRAY_COUNT(Pattern)];
		return Selected == EVectorPhysicsBossAttack::AmmoLaunch && !bAmmoAvailable
			? EVectorPhysicsBossAttack::AerialBurst
			: Selected;
	}
	case EVectorPhysicsBossPhase::Defeated:
	default:
		return EVectorPhysicsBossAttack::None;
	}
}

FString FVectorPhysicsBossState::Describe() const
{
	return FString::Printf(
		TEXT("phase=%s transitions=%d structure=%d/2 staggered=%s resolve=%.1fs mass=%.1f pursuit=%.0f ram=%.2fs recovery=%.2fs maxAdds=%d"),
		*VectorPhysicsBossStateInternal::PhaseToString(Phase), TransitionCount,
		BrokenStructureGroupCount,
		bHasEverStaggered ? TEXT("YES") : TEXT("no"),
		StaggerResolveSecondsRemaining,
		GetEffectivePhysicalMass(), GetPursuitSpeedCmPerSecond(),
		GetRamIntervalSeconds(), GetRecoverySeconds(),
		GetMaximumConcurrentAdds());
}

bool FVectorPhysicsBossState::TransitionTo(const EVectorPhysicsBossPhase NewPhase)
{
	if (GetPhaseRank(NewPhase) <= GetPhaseRank(Phase))
	{
		return false;
	}
	Phase = NewPhase;
	++TransitionCount;
	return true;
}

int32 FVectorPhysicsBossState::GetPhaseRank(const EVectorPhysicsBossPhase InPhase)
{
	switch (InPhase)
	{
	case EVectorPhysicsBossPhase::AnchoredShell: return 0;
	case EVectorPhysicsBossPhase::ExposedShell: return 1;
	case EVectorPhysicsBossPhase::Overload: return 2;
	case EVectorPhysicsBossPhase::Defeated: return 3;
	default: return -1;
	}
}
