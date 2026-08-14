// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Impact/VectorImpactMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorLiftForkMassScalingTest,
	"Vector.Combat.LiftFork.MassScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorLiftForkMassScalingTest::RunTest(const FString& Parameters)
{
	constexpr double BaseLiftSpeed = 1900.0;
	const double LightSpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(BaseLiftSpeed, 1.25);
	const double MediumSpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(BaseLiftSpeed, 2.5);
	const double HeavySpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(BaseLiftSpeed, 5.0);

	TestEqual(TEXT("Light lift speed"), LightSpeed, 1520.0, 1.e-6);
	TestEqual(TEXT("Medium lift speed"), MediumSpeed, 760.0, 1.e-6);
	TestEqual(TEXT("Heavy lift speed"), HeavySpeed, 380.0, 1.e-6);
	TestTrue(TEXT("Mass ordering remains readable"), LightSpeed > MediumSpeed && MediumSpeed > HeavySpeed);
	TestTrue(TEXT("Light and medium can exceed landing-shock threshold"),
		LightSpeed > 600.0 && MediumSpeed > 600.0);
	TestTrue(TEXT("Stable heavy needs stagger or another setup for landing shock"), HeavySpeed < 600.0);
	TestEqual(TEXT("Invalid mass is rejected"),
		FVectorImpactMath::ComputeMassAdjustedSpeed(BaseLiftSpeed, 0.0), 0.0, 1.e-6);
	return true;
}

#endif
