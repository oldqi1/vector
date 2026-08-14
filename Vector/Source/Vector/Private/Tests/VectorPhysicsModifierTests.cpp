// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Physics/VectorPhysicsModifierMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsModifierLayeringTest,
	"Vector.PhysicsModifier.Layering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsModifierLayeringTest::RunTest(const FString& Parameters)
{
	using namespace VectorPhysicsModifierMath;
	TestEqual(TEXT("No modifiers keep normal friction"),
		ComputeEffectiveFrictionMultiplier(1.0, false, 0.35), 1.0, 1.e-6);
	TestEqual(TEXT("Lubricant lowers normal ground friction"),
		ComputeEffectiveFrictionMultiplier(1.0, true, 0.35), 0.35, 1.e-6);
	TestEqual(TEXT("Lower environment layer wins over lubricant"),
		ComputeEffectiveFrictionMultiplier(0.10, true, 0.35), 0.10, 1.e-6);
	TestEqual(TEXT("Leaving environment preserves active lubricant"),
		ComputeEffectiveFrictionMultiplier(1.0, true, 0.35), 0.35, 1.e-6);
	TestEqual(TEXT("Buoyant multiplier lowers gravity"),
		ComputeEffectiveGravityScale(1.0, true, 0.35), 0.35, 1.e-6);
	TestEqual(TEXT("Inactive buoyancy restores base gravity"),
		ComputeEffectiveGravityScale(1.2, false, 0.35), 1.2, 1.e-6);
	TestEqual(TEXT("Invalid negative base gravity cannot invert gravity"),
		ComputeEffectiveGravityScale(-1.0, true, 0.35), 0.0, 1.e-6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
