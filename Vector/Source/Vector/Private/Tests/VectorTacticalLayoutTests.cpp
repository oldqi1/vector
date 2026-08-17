// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PCG/VectorTacticalLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorTacticalLayoutDeterminismTest,
	"Vector.PCG.Layout.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorTacticalLayoutDeterminismTest::RunTest(const FString& Parameters)
{
	const FVectorTacticalLayout First = FVectorTacticalGenerator::Generate(4417);
	const FVectorTacticalLayout Second = FVectorTacticalGenerator::Generate(4417);
	TestTrue(TEXT("Generated layout is valid"), First.bValid);
	TestEqual(TEXT("Same seed resolves identically"), First.Describe(), Second.Describe());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorTacticalLayoutDiversityTest,
	"Vector.PCG.Layout.SeedDiversity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorTacticalLayoutDiversityTest::RunTest(const FString& Parameters)
{
	const FString Baseline = FVectorTacticalGenerator::Generate(1).DescribeModuleSequence();
	bool bFoundDifferentLayout = false;
	for (int32 Seed = 2; Seed <= 32; ++Seed)
	{
		if (FVectorTacticalGenerator::Generate(Seed).DescribeModuleSequence() != Baseline)
		{
			bFoundDifferentLayout = true;
			break;
		}
	}
	TestTrue(TEXT("Different seeds can change the tactical layout"), bFoundDifferentLayout);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorTacticalLayoutConstraintsTest,
	"Vector.PCG.Layout.PhysicsConstraints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorTacticalLayoutConstraintsTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = -32; Seed <= 32; ++Seed)
	{
		const FVectorTacticalLayout Layout = FVectorTacticalGenerator::Generate(Seed);
		TestTrue(FString::Printf(TEXT("Seed %d is valid"), Seed), Layout.bValid);
		TestTrue(FString::Printf(TEXT("Seed %d supports tether setup"), Seed),
			Layout.HasOpportunity(EVectorPhysicsOpportunity::TetherSwingArc));
		TestTrue(FString::Printf(TEXT("Seed %d supports vertical play"), Seed),
			Layout.HasOpportunity(EVectorPhysicsOpportunity::HeightDrop));
		TestTrue(FString::Printf(TEXT("Seed %d has multiple real height layers"), Seed),
			Layout.GetMaximumHeightLayerCount() >= 2);
		TestTrue(FString::Printf(TEXT("Seed %d has a meaningful drop"), Seed),
			Layout.GetMaximumHeightDifferenceCm() >= 150.0);
		TestTrue(FString::Printf(TEXT("Seed %d meets score"), Seed),
			Layout.TacticalScore >= 12.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorTacticalLayoutFallbackTest,
	"Vector.PCG.Layout.ValidatedFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorTacticalLayoutFallbackTest::RunTest(const FString& Parameters)
{
	FVectorTacticalGenerationRules Rules;
	Rules.MaximumGenerationAttempts = 0;
	const FVectorTacticalLayout Layout = FVectorTacticalGenerator::Generate(99, Rules);
	TestTrue(TEXT("Fallback is marked"), Layout.bUsedFallback);
	TestTrue(TEXT("Fallback remains valid"), Layout.bValid);
	TestTrue(TEXT("Fallback reports its fixed sequence"),
		Layout.Describe().Contains(TEXT("OpenBowl>HeightShelf")));
	return true;
}

#endif
