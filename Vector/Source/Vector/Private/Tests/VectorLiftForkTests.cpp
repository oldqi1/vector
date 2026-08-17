// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorLiftForkMath.h"
#include "Misc/AutomationTest.h"
#include "Impact/VectorImpactMath.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorLiftForkVerticalRedirectMathTest,
	"Vector.Combat.LiftFork.VerticalRedirectMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorLiftForkVerticalRedirectMathTest::RunTest(const FString& Parameters)
{
	const FVectorLiftForkRedirectResult Stationary =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector::ZeroVector);
	TestTrue(TEXT("Stationary input is valid"), Stationary.bValid);
	TestTrue(TEXT("Stationary input uses explicit floor"), Stationary.bUsedVerticalFloor);
	TestTrue(TEXT("Stationary output stays below the shock threshold"),
		Stationary.OutputVelocity.Equals(FVector(0.0, 0.0, 520.0), 1.e-6));
	TestTrue(TEXT("Base floor cannot self-trigger a 600cm/s landing shock"),
		Stationary.OutputVelocity.Z < 600.0);
	TestTrue(TEXT("Stationary floor stays inside declared budget"),
		Stationary.IsWithinDeclaredBudget());

	const FVectorLiftForkRedirectResult Slow =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector(800.0, 0.0, 75.0));
	TestEqual(TEXT("800 input lossy budget"), Slow.RedirectBudgetCmPerSecond, 704.0, 1.e-6);
	TestFalse(TEXT("800 input uses its inherited redirect budget"), Slow.bUsedVerticalFloor);
	TestEqual(TEXT("800 input retains horizontal speed"),
		Slow.OutputVelocity.X, 281.6, 1.e-6);
	TestEqual(TEXT("800 input redirects vertical speed"),
		Slow.OutputVelocity.Z, 645.2266578497822, 1.e-6);
	TestTrue(TEXT("800 redirect stays inside declared budget"), Slow.IsWithinDeclaredBudget());

	const FVectorLiftForkRedirectResult Medium =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector(0.0, 1400.0, 0.0));
	TestFalse(TEXT("1400 input uses redirect branch"), Medium.bUsedVerticalFloor);
	TestEqual(TEXT("1400 input lossy budget"), Medium.RedirectBudgetCmPerSecond, 1232.0, 1.e-6);
	TestEqual(TEXT("1400 retained horizontal speed"), Medium.OutputVelocity.Y, 492.8, 1.e-6);
	TestEqual(TEXT("1400 redirected vertical speed"), Medium.OutputVelocity.Z,
		1129.146651237119, 1.e-6);
	TestEqual(TEXT("1400 output magnitude equals lossy budget"),
		Medium.OutputVelocity.Size(), 1232.0, 1.e-6);
	TestTrue(TEXT("1400 redirect stays inside declared budget"), Medium.IsWithinDeclaredBudget());

	const FVectorLiftForkRedirectResult Fast =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector(-2400.0, 0.0, -500.0));
	TestFalse(TEXT("2400 input uses redirect branch"), Fast.bUsedVerticalFloor);
	TestEqual(TEXT("2400 input lossy budget"), Fast.RedirectBudgetCmPerSecond, 2112.0, 1.e-6);
	TestEqual(TEXT("2400 keeps incoming horizontal direction"), Fast.OutputVelocity.X, -844.8, 1.e-6);
	TestEqual(TEXT("2400 redirected vertical speed"), Fast.OutputVelocity.Z,
		1935.6799735493469, 1.e-6);
	TestEqual(TEXT("2400 output magnitude equals lossy budget"),
		Fast.OutputVelocity.Size(), 2112.0, 1.e-6);
	TestTrue(TEXT("2400 redirect cannot increase speed"),
		Fast.OutputVelocity.Size() <= Fast.PreHorizontalSpeedCmPerSecond);
	TestTrue(TEXT("2400 redirect stays inside declared budget"), Fast.IsWithinDeclaredBudget());

	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const FVectorLiftForkRedirectResult Invalid =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector(NaN, 100.0, 0.0));
	TestFalse(TEXT("NaN input is rejected"), Invalid.bValid);
	TestTrue(TEXT("Rejected input cannot leak NaN"), Invalid.OutputVelocity.IsZero());
	TestFalse(TEXT("Rejected input cannot claim budget PASS"), Invalid.IsWithinDeclaredBudget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorLiftForkPriorVerticalVelocityTest,
	"Vector.Combat.LiftFork.PriorVerticalVelocityIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorLiftForkPriorVerticalVelocityTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Grounded planar target uses the ground redirect"),
		FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
			false, FVector(1400.0, 0.0, 0.0)));
	TestTrue(TEXT("A pending lift queued this frame uses the airborne branch"),
		FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
			false, FVector(0.0, 0.0, 700.0)));
	TestTrue(TEXT("A falling target at its apex remains airborne"),
		FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
			true, FVector::ZeroVector));

	const FVectorLiftForkRedirectResult Rising =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector(1400.0, 0.0, 900.0));
	const FVectorLiftForkRedirectResult Falling =
		FVectorLiftForkMath::ComputeVerticalRedirect(FVector(1400.0, 0.0, -900.0));
	TestTrue(TEXT("Prior vertical sign does not alter redirect"),
		Rising.OutputVelocity.Equals(Falling.OutputVelocity, 1.e-6));
	TestEqual(TEXT("Prior vertical speed is replaced by solved vertical speed"),
		Rising.OutputVelocity.Z, 1129.146651237119, 1.e-6);
	TestEqual(TEXT("Incoming planar speed is not mass-scaled a second time"),
		Rising.PreHorizontalSpeedCmPerSecond, 1400.0, 1.e-6);
	TestTrue(TEXT("Replacement remains inside declared redirect budget"),
		Rising.IsWithinDeclaredBudget() && Falling.IsWithinDeclaredBudget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorBallisticArcTest,
	"Vector.Combat.BallisticArc.ReachesLocked3DPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorBallisticArcTest::RunTest(const FString& Parameters)
{
	const FVector Start(0.0, 0.0, 100.0);
	const FVector Target(1200.0, 400.0, 500.0);
	FVector LaunchVelocity = FVector::ZeroVector;
	TestTrue(TEXT("Valid 3D ballistic arc solves"),
		FVectorImpactMath::ComputeBallisticLaunchVelocity(
			Start, Target, 1.2, -980.0, LaunchVelocity));
	const FVector Reached = FVectorImpactMath::SampleBallisticPosition(
		Start, LaunchVelocity, -980.0, 1.2);
	TestTrue(TEXT("Ballistic sample reaches locked elevated point"),
		Reached.Equals(Target, 1.e-6));
	TestTrue(TEXT("Arc has positive launch height"), LaunchVelocity.Z > 0.0);
	TestFalse(TEXT("Zero flight time is rejected"),
		FVectorImpactMath::ComputeBallisticLaunchVelocity(
			Start, Target, 0.0, -980.0, LaunchVelocity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorDirectedSlamMathTest,
	"Vector.Combat.LiftFork.DirectedSlamMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorDirectedSlamMathTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("A tap remains lift-only"),
		FVectorLiftForkMath::IsSlamGestureQualified(
			0.05, 80.0, 0.12, 28.0));
	TestFalse(TEXT("A stationary hold remains lift-only"),
		FVectorLiftForkMath::IsSlamGestureQualified(
			0.50, 10.0, 0.12, 28.0));
	TestTrue(TEXT("A held drag qualifies the continuous slam gesture"),
		FVectorLiftForkMath::IsSlamGestureQualified(
			0.20, 40.0, 0.12, 28.0));

	const FVector Start(0.0, 0.0, 500.0);
	const FVector CurrentVelocity(0.0, 0.0, -500.0);
	const FVectorDirectedSlamResult Reachable =
		FVectorLiftForkMath::ComputeDirectedSlam(
			Start, FVector(100.0, 0.0, 0.0), CurrentVelocity, -980.0,
			FVectorLiftForkMath::DirectedSlamUpgradeFloorCmPerSecond);
	TestTrue(TEXT("Nearby lower point has a feasible downward arc"), Reachable.bValid);
	TestEqual(TEXT("Reachable arc uses deterministic discrete time"),
		Reachable.FlightSeconds, 0.55, 1.e-6);
	TestTrue(TEXT("Reachable arc starts downward"), Reachable.LaunchVelocity.Z < 0.0);
	TestTrue(TEXT("Reachable arc stays inside slam budget"),
		Reachable.IsWithinDeclaredBudget());
	TestTrue(TEXT("Reachable arc lands on the locked 3D point"),
		FVectorImpactMath::SampleBallisticPosition(
			Start, Reachable.LaunchVelocity, -980.0,
			Reachable.FlightSeconds).Equals(FVector(100.0, 0.0, 0.0), 1.e-5));

	const FVectorDirectedSlamResult TooNear =
		FVectorLiftForkMath::ComputeDirectedSlam(
			Start, FVector(0.0, 0.0, 490.0),
			FVector(0.0, 0.0, -200.0), -980.0,
			FVectorLiftForkMath::DirectedSlamUpgradeFloorCmPerSecond);
	TestFalse(TEXT("A point too near for the minimum downward time is rejected"),
		TooNear.bValid);

	const FVectorDirectedSlamResult TooFar =
		FVectorLiftForkMath::ComputeDirectedSlam(
			Start, FVector(1000.0, 0.0, 0.0), CurrentVelocity, -980.0,
			FVectorLiftForkMath::DirectedSlamUpgradeFloorCmPerSecond);
	TestFalse(TEXT("A far point outside the finite speed budget is rejected"),
		TooFar.bValid);

	const FVectorDirectedSlamResult FastFar =
		FVectorLiftForkMath::ComputeDirectedSlam(
			FVector(0.0, 0.0, 700.0), FVector(500.0, 0.0, 0.0),
			FVector(900.0, 0.0, -500.0));
	TestTrue(TEXT("Existing speed makes a farther point reachable"), FastFar.bValid);
	TestEqual(TEXT("Far arc selects its deterministic minimum-speed time"),
		FastFar.FlightSeconds, 0.70, 1.e-6);
	TestTrue(TEXT("Far arc remains budget limited"),
		FastFar.IsWithinDeclaredBudget());

	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const FVectorDirectedSlamResult Invalid =
		FVectorLiftForkMath::ComputeDirectedSlam(
			Start, FVector(NaN, 0.0, 0.0), CurrentVelocity);
	TestFalse(TEXT("Non-finite slam input is rejected"), Invalid.bValid);
	TestTrue(TEXT("Rejected slam cannot leak velocity"),
		Invalid.LaunchVelocity.IsZero());
	return true;
}

#endif
