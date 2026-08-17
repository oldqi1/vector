// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Auditable result of converting existing planar speed into a vertical arc.
 * The floor branch is the lift fork's one explicit energy grant; the redirect
 * branch may only spend the lossy budget derived from the incoming speed.
 */
struct VECTOR_API FVectorLiftForkRedirectResult
{
	bool bValid = false;
	bool bUsedVerticalFloor = false;
	double PreHorizontalSpeedCmPerSecond = 0.0;
	double RedirectBudgetCmPerSecond = 0.0;
	FVector OutputVelocity = FVector::ZeroVector;

	double GetAllowedOutputSpeedCmPerSecond() const;
	bool IsWithinDeclaredBudget(double Tolerance = 1.e-6) const;
};

/** Deterministic result for one budget-limited directed slam arc. */
struct VECTOR_API FVectorDirectedSlamResult
{
	bool bValid = false;
	double SpeedBudgetCmPerSecond = 0.0;
	double NaturalFlightSeconds = 0.0;
	double UpperFlightSeconds = 0.0;
	double FlightSeconds = 0.0;
	int32 FeasibleCandidateCount = 0;
	FVector LaunchVelocity = FVector::ZeroVector;

	bool IsWithinDeclaredBudget(double Tolerance = 1.e-6) const;
};

/** Pure deterministic math shared by lift-fork runtime code and Automation. */
class VECTOR_API FVectorLiftForkMath
{
public:
	/** Fraction of the incoming planar direction retained after redirecting. */
	static constexpr double HorizontalRetention = 1.0 - 0.6;

	/** Loss applied before the remaining speed budget is rotated. */
	static constexpr double RedirectEfficiency = 0.88;

	/** Explicit low-speed lift energy supplied by the tool. */
	/** Base lift stays below the 600cm/s landing-shock threshold. */
	static constexpr double VerticalFloorCmPerSecond = 520.0;

	/** Explicit energy grant owned by the Lift-Vector Coupler rule module. */
	static constexpr double DirectedSlamUpgradeFloorCmPerSecond = 700.0;
	static constexpr double MinimumSlamFlightSeconds = 0.20;
	static constexpr double MaximumSlamFlightSeconds = 0.90;
	static constexpr double SlamFlightStepSeconds = 0.05;
	static constexpr double NaturalFlightFraction = 0.90;

	/**
	 * Routes an already-airborne target (including a launch queued this frame)
	 * away from the ground redirect. This prevents repeated floor grants and
	 * reserves the second use for the directed-slam verb.
	 */
	static bool ShouldRouteToAirborneFollowUp(
		bool bMovementIsFalling,
		const FVector& EffectiveVelocity);

	/** Pure input gate: both hold time and cursor displacement are required. */
	static bool IsSlamGestureQualified(
		double HeldSeconds,
		double DragPixels,
		double MinimumHeldSeconds,
		double MinimumDragPixels);

	/**
	 * Redirects XY speed without looking at mass or the current Z component.
	 * Non-finite input is rejected with a zero, invalid result.
	 */
	static FVectorLiftForkRedirectResult ComputeVerticalRedirect(
		const FVector& CurrentVelocity);

	/**
	 * Finds the lowest-speed discrete ballistic arc that starts downward and
	 * stays within max(|CurrentVelocity|, MinimumSpeedBudget). No velocity component
	 * is rewritten after solving; an unreachable point returns an invalid result.
	 */
	static FVectorDirectedSlamResult ComputeDirectedSlam(
		const FVector& Start,
		const FVector& Target,
		const FVector& CurrentVelocity,
		double GravityZCmPerSecondSquared = -980.0,
		double MinimumSpeedBudgetCmPerSecond = VerticalFloorCmPerSecond);
};
