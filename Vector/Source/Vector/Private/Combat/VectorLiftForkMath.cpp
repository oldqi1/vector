// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorLiftForkMath.h"

#include "Impact/VectorImpactMath.h"

namespace VectorLiftForkMathInternal
{
	bool IsFiniteVector(const FVector& Value)
	{
		return !Value.ContainsNaN()
			&& FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}
}

bool FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
	const bool bMovementIsFalling,
	const FVector& EffectiveVelocity)
{
	if (bMovementIsFalling)
	{
		return true;
	}
	if (EffectiveVelocity.ContainsNaN()
		|| !FMath::IsFinite(EffectiveVelocity.Z))
	{
		return false;
	}
	return FMath::Abs(EffectiveVelocity.Z) > UE_KINDA_SMALL_NUMBER;
}

bool FVectorLiftForkMath::IsSlamGestureQualified(
	const double HeldSeconds,
	const double DragPixels,
	const double MinimumHeldSeconds,
	const double MinimumDragPixels)
{
	return FMath::IsFinite(HeldSeconds)
		&& FMath::IsFinite(DragPixels)
		&& FMath::IsFinite(MinimumHeldSeconds)
		&& FMath::IsFinite(MinimumDragPixels)
		&& HeldSeconds >= FMath::Max(0.0, MinimumHeldSeconds)
		&& DragPixels >= FMath::Max(0.0, MinimumDragPixels);
}

double FVectorLiftForkRedirectResult::GetAllowedOutputSpeedCmPerSecond() const
{
	if (!bValid)
	{
		return 0.0;
	}
	return bUsedVerticalFloor
		? FMath::Max(RedirectBudgetCmPerSecond, FVectorLiftForkMath::VerticalFloorCmPerSecond)
		: RedirectBudgetCmPerSecond;
}

bool FVectorLiftForkRedirectResult::IsWithinDeclaredBudget(const double Tolerance) const
{
	if (!bValid || OutputVelocity.ContainsNaN()
		|| !FMath::IsFinite(OutputVelocity.X)
		|| !FMath::IsFinite(OutputVelocity.Y)
		|| !FMath::IsFinite(OutputVelocity.Z))
	{
		return false;
	}
	return OutputVelocity.Size() <= GetAllowedOutputSpeedCmPerSecond()
		+ FMath::Max(0.0, Tolerance);
}

FVectorLiftForkRedirectResult FVectorLiftForkMath::ComputeVerticalRedirect(
	const FVector& CurrentVelocity)
{
	FVectorLiftForkRedirectResult Result;
	if (CurrentVelocity.ContainsNaN()
		|| !FMath::IsFinite(CurrentVelocity.X)
		|| !FMath::IsFinite(CurrentVelocity.Y)
		|| !FMath::IsFinite(CurrentVelocity.Z))
	{
		return Result;
	}

	const FVector HorizontalVelocity(CurrentVelocity.X, CurrentVelocity.Y, 0.0);
	Result.PreHorizontalSpeedCmPerSecond = HorizontalVelocity.Size();
	Result.RedirectBudgetCmPerSecond =
		Result.PreHorizontalSpeedCmPerSecond * RedirectEfficiency;
	Result.bValid = true;

	const double VerticalFraction = FMath::Sqrt(FMath::Max(
		0.0, 1.0 - FMath::Square(HorizontalRetention)));
	const double RedirectedVerticalSpeed =
		Result.RedirectBudgetCmPerSecond * VerticalFraction;
	if (RedirectedVerticalSpeed < VerticalFloorCmPerSecond)
	{
		Result.bUsedVerticalFloor = true;
		Result.OutputVelocity = FVector(0.0, 0.0, VerticalFloorCmPerSecond);
		return Result;
	}

	const FVector HorizontalDirection = HorizontalVelocity.GetSafeNormal();
	Result.OutputVelocity =
		HorizontalDirection * (Result.RedirectBudgetCmPerSecond * HorizontalRetention)
		+ FVector::UpVector * RedirectedVerticalSpeed;
	return Result;
}

bool FVectorDirectedSlamResult::IsWithinDeclaredBudget(
	const double Tolerance) const
{
	return bValid
		&& VectorLiftForkMathInternal::IsFiniteVector(LaunchVelocity)
		&& FMath::IsFinite(SpeedBudgetCmPerSecond)
		&& SpeedBudgetCmPerSecond >= 0.0
		&& LaunchVelocity.Z < 0.0
		&& LaunchVelocity.Size() <= SpeedBudgetCmPerSecond
			+ FMath::Max(0.0, Tolerance);
}

FVectorDirectedSlamResult FVectorLiftForkMath::ComputeDirectedSlam(
	const FVector& Start,
	const FVector& Target,
	const FVector& CurrentVelocity,
	const double GravityZCmPerSecondSquared,
	const double MinimumSpeedBudgetCmPerSecond)
{
	FVectorDirectedSlamResult Result;
	if (!VectorLiftForkMathInternal::IsFiniteVector(Start)
		|| !VectorLiftForkMathInternal::IsFiniteVector(Target)
		|| !VectorLiftForkMathInternal::IsFiniteVector(CurrentVelocity)
		|| !FMath::IsFinite(GravityZCmPerSecondSquared)
		|| !FMath::IsFinite(MinimumSpeedBudgetCmPerSecond)
		|| GravityZCmPerSecondSquared >= 0.0)
	{
		return Result;
	}

	const double GravityMagnitude = -GravityZCmPerSecondSquared;
	const double HeightDrop = FMath::Max(0.0, Start.Z - Target.Z);
	const double Discriminant = FMath::Square(CurrentVelocity.Z)
		+ 2.0 * GravityMagnitude * HeightDrop;
	if (!FMath::IsFinite(Discriminant) || Discriminant < 0.0)
	{
		return Result;
	}

	Result.SpeedBudgetCmPerSecond = FMath::Max(
		CurrentVelocity.Size(), FMath::Max(0.0, MinimumSpeedBudgetCmPerSecond));
	Result.NaturalFlightSeconds =
		(CurrentVelocity.Z + FMath::Sqrt(Discriminant)) / GravityMagnitude;
	Result.UpperFlightSeconds = FMath::Clamp(
		NaturalFlightFraction * Result.NaturalFlightSeconds,
		MinimumSlamFlightSeconds,
		MaximumSlamFlightSeconds);
	if (!FMath::IsFinite(Result.SpeedBudgetCmPerSecond)
		|| !FMath::IsFinite(Result.NaturalFlightSeconds)
		|| !FMath::IsFinite(Result.UpperFlightSeconds))
	{
		return FVectorDirectedSlamResult();
	}

	constexpr double ComparisonTolerance = 1.e-6;
	const int32 MaximumCandidateSteps = FMath::FloorToInt(
		(Result.UpperFlightSeconds - MinimumSlamFlightSeconds)
		/ SlamFlightStepSeconds + ComparisonTolerance);
	double BestSpeed = TNumericLimits<double>::Max();
	for (int32 StepIndex = 0; StepIndex <= MaximumCandidateSteps; ++StepIndex)
	{
		const double FlightSeconds = MinimumSlamFlightSeconds
			+ StepIndex * SlamFlightStepSeconds;
		FVector CandidateVelocity = FVector::ZeroVector;
		if (!FVectorImpactMath::ComputeBallisticLaunchVelocity(
			Start, Target, FlightSeconds, GravityZCmPerSecondSquared,
			CandidateVelocity)
			|| CandidateVelocity.Z >= 0.0)
		{
			continue;
		}

		const double CandidateSpeed = CandidateVelocity.Size();
		if (!FMath::IsFinite(CandidateSpeed)
			|| CandidateSpeed > Result.SpeedBudgetCmPerSecond
				+ ComparisonTolerance)
		{
			continue;
		}
		++Result.FeasibleCandidateCount;
		if (CandidateSpeed < BestSpeed - ComparisonTolerance
			|| (FMath::IsNearlyEqual(CandidateSpeed, BestSpeed,
				ComparisonTolerance)
				&& FlightSeconds < Result.FlightSeconds))
		{
			Result.bValid = true;
			Result.FlightSeconds = FlightSeconds;
			Result.LaunchVelocity = CandidateVelocity;
			BestSpeed = CandidateSpeed;
		}
	}
	return Result;
}
