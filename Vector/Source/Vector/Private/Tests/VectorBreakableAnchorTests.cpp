// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorBreakableAnchorComponent.h"
#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorBreakableAnchorLedgerTest,
	"Vector.Structure.AnchorGroups.DiscreteCollisionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorBreakableAnchorLedgerTest::RunTest(const FString& Parameters)
{
	FVectorAnchorStructureLedger Ledger;
	Ledger.MinimumClosingSpeedCmPerSecond = 700.0;
	Ledger.MinimumLateralAlignment = 0.55;

	const FVectorAnchorStructureResult Slow = Ledger.ApplyCollisionEvent(699.0, 1.0);
	TestFalse(TEXT("Slow collision does not become structure damage"), Slow.bAccepted);
	TestEqual(TEXT("Slow collision breaks no group"), Ledger.GetBrokenGroupCount(), 0);

	const FVectorAnchorStructureResult Frontal = Ledger.ApplyCollisionEvent(900.0, 0.2);
	TestFalse(TEXT("Fast frontal collision still fails the side condition"), Frontal.bAccepted);
	TestEqual(TEXT("Frontal collision breaks no group"), Ledger.GetBrokenGroupCount(), 0);

	const FVectorAnchorStructureResult Left = Ledger.ApplyCollisionEvent(900.0, -0.9);
	TestTrue(TEXT("Left lateral collision is accepted"), Left.bAccepted);
	TestTrue(TEXT("Left lateral collision breaks one discrete group"), Left.bBrokeGroup);
	TestEqual(TEXT("One group broken"), Ledger.GetBrokenGroupCount(), 1);
	TestFalse(TEXT("One group is unstable, not launchable"), Ledger.IsLaunchable());

	const FVectorAnchorStructureResult Duplicate = Ledger.ApplyCollisionEvent(1200.0, -1.0);
	TestTrue(TEXT("Repeated qualified event is understood"), Duplicate.bAccepted);
	TestFalse(TEXT("Repeated same-side event cannot fill a hidden bar"), Duplicate.bBrokeGroup);
	TestEqual(TEXT("Still only one group broken"), Ledger.GetBrokenGroupCount(), 1);

	const FVectorAnchorStructureResult Right = Ledger.ApplyCollisionEvent(850.0, 0.8);
	TestTrue(TEXT("Opposite-side event breaks the second group"), Right.bBrokeGroup);
	TestEqual(TEXT("Both groups broken"), Ledger.GetBrokenGroupCount(), 2);
	TestTrue(TEXT("Two distinct structure events make target launchable"), Ledger.IsLaunchable());

	const FVectorAnchorStructureResult NonFinite = Ledger.ApplyCollisionEvent(
		std::numeric_limits<double>::quiet_NaN(), 1.0);
	TestFalse(TEXT("Non-finite input is rejected safely"), NonFinite.bAccepted);
	return true;
}

#endif
