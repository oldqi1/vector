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
}

bool FVectorPhysicsBossState::ApplyHealthRatio(const double HealthRatio)
{
	if (!FMath::IsFinite(HealthRatio))
	{
		return false;
	}

	const double ClampedRatio = FMath::Clamp(HealthRatio, 0.0, 1.0);
	if (ClampedRatio <= 0.0)
	{
		return TransitionTo(EVectorPhysicsBossPhase::Defeated);
	}
	if (ClampedRatio <= FMath::Clamp(Rules.OverloadHealthRatio, 0.0, 1.0))
	{
		return TransitionTo(EVectorPhysicsBossPhase::Overload);
	}
	if (bHasEverStaggered
		|| ClampedRatio <= FMath::Clamp(Rules.ExposedHealthRatio, 0.0, 1.0))
	{
		return TransitionTo(EVectorPhysicsBossPhase::ExposedShell);
	}
	return false;
}

bool FVectorPhysicsBossState::NotifyStaggered()
{
	bHasEverStaggered = true;
	return TransitionTo(EVectorPhysicsBossPhase::ExposedShell);
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

FString FVectorPhysicsBossState::Describe() const
{
	return FString::Printf(
		TEXT("phase=%s transitions=%d staggered=%s mass=%.1f ram=%.2fs recovery=%.2fs maxAdds=%d"),
		*VectorPhysicsBossStateInternal::PhaseToString(Phase), TransitionCount,
		bHasEverStaggered ? TEXT("YES") : TEXT("no"),
		GetEffectivePhysicalMass(), GetRamIntervalSeconds(), GetRecoverySeconds(),
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
