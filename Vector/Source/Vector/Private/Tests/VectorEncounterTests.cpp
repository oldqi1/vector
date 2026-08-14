// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Hunt/VectorEncounterComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorEncounterLedgerTest,
	"Vector.Hunt.EncounterLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorEncounterLedgerTest::RunTest(const FString& Parameters)
{
	UVectorEncounterComponent* Encounter = NewObject<UVectorEncounterComponent>();
	TestNotNull(TEXT("Encounter component can be constructed"), Encounter);
	if (!Encounter)
	{
		return false;
	}

	Encounter->StartEncounter(3);
	TestEqual(TEXT("Three enemies registered"), Encounter->GetRemainingEnemies(), 3);
	TestEqual(TEXT("Contract begins active"), Encounter->GetEncounterState(), EVectorEncounterState::Active);

	Encounter->NotifyEnemyDefeated();
	Encounter->NotifyEnemyDefeated();
	TestEqual(TEXT("Two defeats leave one"), Encounter->GetRemainingEnemies(), 1);
	TestFalse(TEXT("Contract not early-complete"), Encounter->IsComplete());

	Encounter->NotifyEnemyDefeated();
	TestEqual(TEXT("Final defeat reaches zero"), Encounter->GetRemainingEnemies(), 0);
	TestTrue(TEXT("Final defeat completes contract"), Encounter->IsComplete());
	TestTrue(TEXT("Completed encounter duration is non-negative"), Encounter->GetElapsedSeconds() >= 0.0);

	Encounter->NotifyEnemyDefeated();
	TestEqual(TEXT("Extra defeat cannot underflow"), Encounter->GetRemainingEnemies(), 0);

	Encounter->StartEncounter(0);
	TestEqual(TEXT("Empty contract remains inactive"), Encounter->GetEncounterState(), EVectorEncounterState::Inactive);
	TestFalse(TEXT("Empty contract is not a false completion"), Encounter->IsComplete());
	return true;
}

#endif
