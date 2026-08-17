// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Environment/VectorEnvironmentalRedirector.h"
#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorEnvironmentalRedirectorMathTest,
	"Vector.Environment.Redirector.DeterministicBudgetAndGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorEnvironmentalRedirectorMathTest::RunTest(const FString& Parameters)
{
	const FVectorEnvironmentalRedirectResult Horizontal =
		FVectorEnvironmentalRedirectMath::ComputeRedirect(
			FVector(600.0, 800.0, 0.0), FVector::RightVector, 0.88);
	TestTrue(TEXT("Valid redirect solves"), Horizontal.bValid);
	TestEqual(TEXT("Redirect reads full incoming speed"),
		Horizontal.InputSpeedCmPerSecond, 1000.0, 1.e-6);
	TestEqual(TEXT("Redirect applies one lossy efficiency"),
		Horizontal.OutputSpeedCmPerSecond, 880.0, 1.e-6);
	TestTrue(TEXT("Redirect follows the configured exit direction"),
		Horizontal.OutputVelocity.Equals(FVector(0.0, 880.0, 0.0), 1.e-6));
	TestTrue(TEXT("Redirect never increases speed"), Horizontal.IsWithinInputBudget());

	const FVector ExitDirection(1.0, 0.0, 1.0);
	const FVectorEnvironmentalRedirectResult Vertical =
		FVectorEnvironmentalRedirectMath::ComputeRedirect(
			FVector(0.0, -1200.0, 500.0), ExitDirection, 0.75);
	TestTrue(TEXT("3D exit direction solves"), Vertical.bValid);
	TestEqual(TEXT("3D redirect still reads the full incoming budget"),
		Vertical.InputSpeedCmPerSecond, 1300.0, 1.e-6);
	TestTrue(TEXT("3D output direction matches the normalized exit"),
		Vertical.OutputVelocity.GetSafeNormal().Equals(
			ExitDirection.GetSafeNormal(), 1.e-6));
	TestTrue(TEXT("3D redirect stays budget limited"), Vertical.IsWithinInputBudget());

	TestFalse(TEXT("Walking cannot trigger the converter"),
		FVectorEnvironmentalRedirectMath::ShouldConsume(false, false, 900.0, 220.0));
	TestFalse(TEXT("An overlap cannot be consumed twice"),
		FVectorEnvironmentalRedirectMath::ShouldConsume(true, true, 900.0, 220.0));
	TestFalse(TEXT("Residual drift below the threshold is ignored"),
		FVectorEnvironmentalRedirectMath::ShouldConsume(true, false, 219.9, 220.0));
	TestTrue(TEXT("A new impulse-driven entry is consumed once"),
		FVectorEnvironmentalRedirectMath::ShouldConsume(true, false, 220.0, 220.0));

	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const FVectorEnvironmentalRedirectResult InvalidInput =
		FVectorEnvironmentalRedirectMath::ComputeRedirect(
			FVector(NaN, 0.0, 0.0), FVector::ForwardVector, 0.88);
	TestFalse(TEXT("Non-finite input is rejected"), InvalidInput.bValid);
	TestTrue(TEXT("Rejected input cannot leak velocity"),
		InvalidInput.OutputVelocity.IsZero());
	TestFalse(TEXT("Efficiency above one is rejected"),
		FVectorEnvironmentalRedirectMath::ComputeRedirect(
			FVector(1000.0, 0.0, 0.0), FVector::ForwardVector, 1.01).bValid);
	return true;
}

#endif
