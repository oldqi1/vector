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

	FVectorTacticalGenerationRules SandboxRules;
	SandboxRules.bRequireHeightShelfAsFinalEncounter = false;
	bool bFoundValidNonDemoRouteWithoutHeightShelf = false;
	for (int32 Seed = 1; Seed <= 64; ++Seed)
	{
		const FVectorTacticalLayout Layout =
			FVectorTacticalGenerator::Generate(Seed, SandboxRules);
		const bool bHasHeightShelf = Layout.Modules.ContainsByPredicate(
			[](const FVectorTacticalModuleDefinition& Module)
			{
				return Module.Type == EVectorTacticalModuleType::HeightShelf;
			});
		if (Layout.bValid && !bHasHeightShelf)
		{
			bFoundValidNonDemoRouteWithoutHeightShelf = true;
			break;
		}
	}
	TestTrue(TEXT("Disabling the Demo route gate preserves the broader PCG framework"),
		bFoundValidNonDemoRouteWithoutHeightShelf);
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
		bool bEncounterOwnsHeightRecipe = false;
		for (const FVectorTacticalModuleDefinition& Module : Layout.Modules)
		{
			if (Module.Type != EVectorTacticalModuleType::BossRing
				&& Module.HasOpportunity(EVectorPhysicsOpportunity::HeightDrop))
			{
				bEncounterOwnsHeightRecipe = Module.HeightLayerCount >= 2
					&& Module.MaximumHeightDifferenceCm >= 150.0;
			}
		}
		TestTrue(FString::Printf(TEXT("Seed %d is valid"), Seed), Layout.bValid);
		TestTrue(FString::Printf(TEXT("Seed %d supports tether setup"), Seed),
			Layout.HasOpportunity(EVectorPhysicsOpportunity::TetherSwingArc));
		TestTrue(FString::Printf(TEXT("Seed %d supports vertical play"), Seed),
			Layout.HasOpportunity(EVectorPhysicsOpportunity::HeightDrop));
		TestTrue(FString::Printf(TEXT("Seed %d has multiple real height layers"), Seed),
			Layout.GetMaximumHeightLayerCount() >= 2);
		TestTrue(FString::Printf(TEXT("Seed %d has a meaningful drop"), Seed),
			Layout.GetMaximumHeightDifferenceCm() >= 150.0);
		TestTrue(FString::Printf(
			TEXT("Seed %d gives an encounter—not only the Boss—a vertical recipe"), Seed),
			bEncounterOwnsHeightRecipe);
		TestTrue(FString::Printf(TEXT("Seed %d meets score"), Seed),
			Layout.TacticalScore >= 12.0);
		TestEqual(FString::Printf(TEXT("Seed %d keeps two demo encounters"), Seed),
			Layout.Modules.Num(), 5);
		TestTrue(FString::Printf(TEXT("Seed %d teaches HeightShelf immediately before Boss"), Seed),
			Layout.Modules[2].Type == EVectorTacticalModuleType::HeightShelf
			&& Layout.Modules[3].Type == EVectorTacticalModuleType::BossRing);
	}
	const TArray<FVectorTacticalModuleDefinition>& Catalog =
		FVectorTacticalGenerator::GetEncounterModuleCatalog();
	TestEqual(TEXT("OpenBowl stays deliberately flat"), Catalog[0].HeightLayerCount, 1);
	TestEqual(TEXT("SlickCross stays deliberately flat"), Catalog[3].HeightLayerCount, 1);
	FVectorTacticalLayout WrongOrder = FVectorTacticalGenerator::Generate(4417);
	WrongOrder.Modules.Swap(1, 2);
	WrongOrder.TacticalScore = FVectorTacticalGenerator::ComputeTacticalScore(WrongOrder);
	FString WrongOrderReason;
	TestFalse(TEXT("A HeightShelf placed before the first encounter fails the demo route gate"),
		FVectorTacticalGenerator::Validate(
			WrongOrder, FVectorTacticalGenerationRules(), WrongOrderReason));
	TestTrue(TEXT("Wrong-order failure explains the HeightShelf route contract"),
		WrongOrderReason.Contains(TEXT("HeightShelf")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorTacticalCircuitContractTest,
	"Vector.PCG.Layout.CombatCircuitContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorTacticalCircuitContractTest::RunTest(const FString& Parameters)
{
	const TArray<FVectorTacticalModuleDefinition>& Catalog =
		FVectorTacticalGenerator::GetEncounterModuleCatalog();
	for (const FVectorTacticalModuleDefinition& Module : Catalog)
	{
		TestTrue(FString::Printf(TEXT("%s owns source/converter/receiver/recovery"),
			*Module.ModuleId.ToString()), Module.HasCompleteCircuit());
		TestTrue(FString::Printf(TEXT("%s exposes two opening verbs"),
			*Module.ModuleId.ToString()), Module.HasDistinctOpenings());
		TestTrue(FString::Printf(TEXT("%s exposes two recipes"),
			*Module.ModuleId.ToString()), Module.Recipes.Num() >= 2);

		FVectorTacticalModuleDefinition WithoutConverter = Module;
		WithoutConverter.Converters.Reset();
		TestFalse(FString::Printf(TEXT("%s loses its combat circuit when the converter is removed"),
			*Module.ModuleId.ToString()), WithoutConverter.HasCompleteCircuit());
	}
	const FVectorTacticalModuleDefinition& HeightShelf = Catalog[2];
	TestTrue(TEXT("HeightShelf route A owns the environment redirector"),
		HeightShelf.Recipes[0].Contains(TEXT("EnvironmentRedirector")));
	TestTrue(TEXT("HeightShelf route B owns lift then directed slam"),
		HeightShelf.Recipes[1].Contains(TEXT("LiftFork>DirectedSlam")));
	TestTrue(TEXT("HeightShelf routes start with different player decisions"),
		HeightShelf.Recipes[0].StartsWith(TEXT("BaitCharge>"))
		&& HeightShelf.Recipes[1].StartsWith(TEXT("VectorInject>")));
	FVectorTacticalModuleDefinition DuplicateOpening = HeightShelf;
	DuplicateOpening.Recipes[1] = TEXT("BaitCharge>LiftFork>DirectedSlam>LowerCrowd");
	TestFalse(TEXT("Recipe data—not a parallel label list—rejects duplicate openings"),
		DuplicateOpening.HasDistinctOpenings());
	const FName EnvironmentRedirector(TEXT("EnvironmentRedirector"));
	const FName LiftFork(TEXT("LiftFork"));
	TestEqual(TEXT("Removing the environment converter deletes exactly one route"),
		HeightShelf.CountRecipesUsingNode(EnvironmentRedirector), 1);
	TestEqual(TEXT("Environment deletion leaves the tool route available"),
		HeightShelf.CountRecipesRemainingWithoutNode(EnvironmentRedirector), 1);
	TestTrue(TEXT("Environment deletion satisfies the non-softlock fallback gate"),
		HeightShelf.HasRouteDeletionFallback(EnvironmentRedirector));
	TestEqual(TEXT("Disabling Lift Fork deletes exactly one route"),
		HeightShelf.CountRecipesUsingNode(LiftFork), 1);
	TestEqual(TEXT("Lift Fork deletion leaves the environment route available"),
		HeightShelf.CountRecipesRemainingWithoutNode(LiftFork), 1);
	TestTrue(TEXT("Lift Fork deletion satisfies the non-softlock fallback gate"),
		HeightShelf.HasRouteDeletionFallback(LiftFork));
	return true;
}

#endif
