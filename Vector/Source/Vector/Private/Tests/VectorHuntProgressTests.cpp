// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Hunt/VectorHuntProgressComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorHuntOrganLedgerTest,
	"Vector.Hunt.OrganLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorHuntOrganLedgerTest::RunTest(const FString& Parameters)
{
	UVectorHuntProgressComponent* Ledger = NewObject<UVectorHuntProgressComponent>();
	TestNotNull(TEXT("Hunt ledger can be constructed"), Ledger);
	TestEqual(TEXT("Fresh hunt starts with zero organs"), Ledger->GetCollectedOrgans(), 0);
	TestEqual(TEXT("First pickup increments total"), Ledger->CollectOrgans(), 1);
	TestEqual(TEXT("Stack pickup increments by amount"), Ledger->CollectOrgans(3), 4);
	TestEqual(TEXT("Zero pickup is ignored"), Ledger->CollectOrgans(0), 4);
	TestEqual(TEXT("Negative pickup is ignored"), Ledger->CollectOrgans(-5), 4);
	Ledger->ResetProgress();
	TestEqual(TEXT("Reset clears run organs"), Ledger->GetCollectedOrgans(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
