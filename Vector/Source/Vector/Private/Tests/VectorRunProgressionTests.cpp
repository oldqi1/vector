// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/VectorRunProgressionTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorRunProgressionVerticalGrowthTest,
	"Vector.Progression.VerticalGrowth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorRunProgressionVerticalGrowthTest::RunTest(const FString& Parameters)
{
	FVectorRunCalibrationState State;
	TestTrue(TEXT("range applies"), State.Apply(EVectorCalibrationType::Range));
	TestTrue(TEXT("impulse applies"), State.Apply(EVectorCalibrationType::Impulse));
	TestTrue(TEXT("capacity applies"), State.Apply(EVectorCalibrationType::Capacity));
	TestTrue(TEXT("recharge applies"), State.Apply(EVectorCalibrationType::Recharge));
	TestEqual(TEXT("range level"), State.RangeLevel, 1);
	TestEqual(TEXT("impulse level"), State.ImpulseLevel, 1);
	TestEqual(TEXT("extra cell"), State.GetAdditionalCells(), 1);
	TestTrue(TEXT("range grows 15 percent"),
		FMath::IsNearlyEqual(State.GetRangeMultiplier(), 1.15));
	TestTrue(TEXT("impulse grows 15 percent"),
		FMath::IsNearlyEqual(State.GetImpulseMultiplier(), 1.15));
	TestTrue(TEXT("recharge interval is shorter"),
		State.GetRechargeIntervalMultiplier() < 1.0);
	TestEqual(TEXT("cross-tool module has a readable offer label"),
		LexToString(EVectorRunModuleType::LiftVectorCoupler),
		FString(TEXT("LIFT-VECTOR COUPLER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorRunProgressionCapTest,
	"Vector.Progression.LevelCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorRunProgressionCapTest::RunTest(const FString& Parameters)
{
	FVectorRunCalibrationState State;
	TestTrue(TEXT("first level"), State.Apply(EVectorCalibrationType::Impulse, 2));
	TestTrue(TEXT("second level"), State.Apply(EVectorCalibrationType::Impulse, 2));
	TestFalse(TEXT("third level rejected"), State.Apply(EVectorCalibrationType::Impulse, 2));
	TestEqual(TEXT("capped at two"), State.ImpulseLevel, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorRunProgressionIndependentAxesTest,
	"Vector.Progression.IndependentAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorRunProgressionIndependentAxesTest::RunTest(const FString& Parameters)
{
	FVectorRunCalibrationState State;
	State.Apply(EVectorCalibrationType::Range);
	State.Apply(EVectorCalibrationType::Range);
	TestTrue(TEXT("range may grow without impulse"),
		FMath::IsNearlyEqual(State.GetRangeMultiplier(), 1.30));
	TestTrue(TEXT("impulse remains baseline"),
		FMath::IsNearlyEqual(State.GetImpulseMultiplier(), 1.0));
	TestEqual(TEXT("cells remain baseline"), State.GetAdditionalCells(), 0);
	return true;
}

#endif
