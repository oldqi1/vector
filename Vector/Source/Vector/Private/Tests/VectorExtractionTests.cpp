// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Hunt/VectorHuntProgressComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorExtractionLedgerTest,
	"Vector.Hunt.ExtractionLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorExtractionLedgerTest::RunTest(const FString& Parameters)
{
	UVectorHuntProgressComponent* Hunt = NewObject<UVectorHuntProgressComponent>();
	TestNotNull(TEXT("Hunt ledger can be constructed"), Hunt);
	if (!Hunt)
	{
		return false;
	}

	Hunt->CollectOrgans(4);
	TestTrue(TEXT("First extraction completes"), Hunt->CompleteExtraction());
	TestTrue(TEXT("Extraction state is final"), Hunt->IsExtractionComplete());
	TestEqual(TEXT("Extraction snapshots organs"), Hunt->GetSecuredOrgans(), 4);
	TestFalse(TEXT("Second extraction is rejected"), Hunt->CompleteExtraction());
	TestEqual(TEXT("Post-extraction collection is frozen"), Hunt->CollectOrgans(2), 4);

	Hunt->ResetProgress();
	TestFalse(TEXT("Reset begins a fresh hunt"), Hunt->IsExtractionComplete());
	TestEqual(TEXT("Reset clears secured organs"), Hunt->GetSecuredOrgans(), 0);
	TestEqual(TEXT("Fresh hunt can collect"), Hunt->CollectOrgans(1), 1);
	return true;
}

#endif
