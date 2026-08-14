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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorDynamicEncounterLedgerTest,
	"Vector.Hunt.DynamicEncounterLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorDynamicEncounterLedgerTest::RunTest(const FString& Parameters)
{
	UVectorEncounterComponent* Encounter = NewObject<UVectorEncounterComponent>();
	TestNotNull(TEXT("Dynamic encounter component can be constructed"), Encounter);
	if (!Encounter)
	{
		return false;
	}

	Encounter->BeginDynamicEncounter();
	TestEqual(TEXT("Dynamic contract starts active at zero"),
		Encounter->GetEncounterState(), EVectorEncounterState::Active);
	TestFalse(TEXT("Unsealed zero is not complete"), Encounter->IsComplete());
	TestTrue(TEXT("First wave accepted"), Encounter->AddEncounterEnemies(2));
	Encounter->NotifyEnemyDefeated();
	Encounter->NotifyEnemyDefeated();
	TestEqual(TEXT("Between waves remaining reaches zero"), Encounter->GetRemainingEnemies(), 0);
	TestFalse(TEXT("Between waves does not complete"), Encounter->IsComplete());
	TestTrue(TEXT("Second wave accepted after zero gap"), Encounter->AddEncounterEnemies(1));
	Encounter->SealDynamicEncounter();
	TestFalse(TEXT("Sealed contract waits for final enemy"), Encounter->IsComplete());
	Encounter->NotifyEnemyDefeated();
	TestTrue(TEXT("Final sealed wave completes"), Encounter->IsComplete());
	TestEqual(TEXT("Total is cumulative across waves"), Encounter->GetTotalEnemies(), 3);
	TestFalse(TEXT("Completed contract rejects another wave"), Encounter->AddEncounterEnemies(1));
	return true;
}

#endif
