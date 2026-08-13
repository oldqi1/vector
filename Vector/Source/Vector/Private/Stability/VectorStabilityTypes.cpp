// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stability/VectorStabilityTypes.h"

FVectorStabilityLedger::FHitResult FVectorStabilityLedger::ReceiveImpactHit(
	const double BaseStaggerDamage,
	const double MassMultiplier,
	const double CollisionTypeMultiplier)
{
	FHitResult Result;
	if (!FMath::IsFinite(BaseStaggerDamage)
		|| !FMath::IsFinite(MassMultiplier)
		|| !FMath::IsFinite(CollisionTypeMultiplier))
	{
		return Result;
	}

	Result.bAccepted = true;

	// 基础失衡量按上下限收敛，再叠加弱点/质量/碰撞类型乘数（对应 Morphorbit ResolveWeakpointHit 的 Clamp*Multiplier）。
	const double AppliedDamage = FMath::Clamp(
		BaseStaggerDamage,
		MinimumBaseStaggerDamage,
		MaximumBaseStaggerDamage)
		* WeakpointStaggerMultiplier
		* MassMultiplier
		* CollisionTypeMultiplier;
	Result.AppliedStabilityDamage = AppliedDamage;

	Stability -= AppliedDamage;
	if (Stability <= 0.0)
	{
		// 归零触发失衡：稳定度重置到上限，进入短促失衡硬直，随后自动倒地。
		Stability = MaximumStability;
		Result.bTriggeredStagger = true;
		EnterState(EVectorStabilityState::Unbalanced, UnbalancedDurationSeconds);
	}
	return Result;
}

void FVectorStabilityLedger::AdvanceState(const double DeltaSeconds)
{
	if (State == EVectorStabilityState::Stable)
	{
		return;
	}
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
	{
		return;
	}

	// 用循环消费帧时间，长帧可跨越多个状态（对齐 Morphorbit AdvanceTimeline 的长帧语义）；
	// 避免卡顿帧把剩余时间丢弃，导致倒地时长被变相延长。
	double RemainingSeconds = DeltaSeconds;
	while (State != EVectorStabilityState::Stable && RemainingSeconds > 0.0)
	{
		if (StateSecondsRemaining > RemainingSeconds)
		{
			StateSecondsRemaining -= RemainingSeconds;
			RemainingSeconds = 0.0;
			break;
		}

		RemainingSeconds -= StateSecondsRemaining;
		switch (State)
		{
		case EVectorStabilityState::Unbalanced:
			EnterState(EVectorStabilityState::Downed, DownedDurationSeconds);
			break;
		case EVectorStabilityState::Downed:
			EnterState(EVectorStabilityState::Rising, RisingDurationSeconds);
			break;
		case EVectorStabilityState::Rising:
			EnterState(EVectorStabilityState::Stable, 0.0);
			break;
		default:
			RemainingSeconds = 0.0;
			break;
		}
	}
}

void FVectorStabilityLedger::Reset()
{
	Stability = MaximumStability;
	State = EVectorStabilityState::Stable;
	StateSecondsRemaining = 0.0;
}

void FVectorStabilityLedger::EnterState(const EVectorStabilityState NewState, const double Seconds)
{
	State = NewState;
	StateSecondsRemaining = FMath::Max(0.0, Seconds);
}
