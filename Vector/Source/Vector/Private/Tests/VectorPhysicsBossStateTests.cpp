// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Boss/VectorPhysicsBossState.h"
#include "Boss/VectorKineticOrb.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsBossInitialStateTest,
	"Vector.Boss.State.InitialContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsBossInitialStateTest::RunTest(const FString& Parameters)
{
	const FVectorPhysicsBossState State;
	TestEqual(TEXT("Boss begins anchored"), State.GetPhase(), EVectorPhysicsBossPhase::AnchoredShell);
	TestEqual(TEXT("Anchored mass is physical, not infinite"), State.GetEffectivePhysicalMass(), 8.0);
	TestEqual(TEXT("Anchored Boss still pursues instead of waiting at center"),
		State.GetPursuitSpeedCmPerSecond(), 210.0);
	TestEqual(TEXT("Anchored add budget"), State.GetMaximumConcurrentAdds(), 2);
	TestTrue(TEXT("First add is allowed"), State.CanSpawnAdd(0));
	const FVector Turned = FVectorWeakGuidanceMath::TurnDirection(
		FVector::ForwardVector, FVector::RightVector, 80.0, 0.5);
	const double TurnDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(FVector::ForwardVector, Turned), -1.0, 1.0)));
	TestTrue(TEXT("Weak guidance obeys its finite turn budget"),
		FMath::IsNearlyEqual(TurnDegrees, 40.0, 0.25));
	const FVector CompletedTurn = FVectorWeakGuidanceMath::TurnDirection(
		FVector::ForwardVector, FVector::RightVector, 80.0, 2.0);
	TestTrue(TEXT("Weak guidance reaches target without overshoot"),
		CompletedTurn.Equals(FVector::RightVector, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsBossStructureExposeTest,
	"Vector.Boss.State.StructureDrivesPhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsBossStructureExposeTest::RunTest(const FString& Parameters)
{
	FVectorPhysicsBossState State;
	TestFalse(TEXT("Stagger alone cannot bypass shell structure"), State.NotifyStaggered());
	TestEqual(TEXT("Stagger leaves shell anchored"), State.GetPhase(), EVectorPhysicsBossPhase::AnchoredShell);
	TestTrue(TEXT("First stagger opens a finite resolve window"), State.TryBeginStaggerResolve(4.5));
	TestFalse(TEXT("Repeated stagger is rejected during resolve"), State.TryBeginStaggerResolve(4.5));
	State.AdvanceStaggerResolve(4.49);
	TestTrue(TEXT("Resolve remains active before its exact expiry"), State.IsStaggerResolveActive());
	State.AdvanceStaggerResolve(0.02);
	TestFalse(TEXT("Resolve expires deterministically"), State.IsStaggerResolveActive());
	TestTrue(TEXT("Stagger becomes available after resolve"), State.TryBeginStaggerResolve(4.5));
	TestTrue(TEXT("First discrete shell event changes phase"), State.NotifyStructureBroken(1));
	TestEqual(TEXT("One shell group exposes one side"), State.GetPhase(), EVectorPhysicsBossPhase::ExposedShell);
	TestEqual(TEXT("Exposed mass falls through the same physical rule"), State.GetEffectivePhysicalMass(), 4.0);
	const double ExposedPursuitSpeed = State.GetPursuitSpeedCmPerSecond();
	TestTrue(TEXT("Breaking one anchor makes pursuit faster"), ExposedPursuitSpeed > 210.0);
	TestFalse(TEXT("Repeated first-group report is idempotent"), State.NotifyStructureBroken(1));
	TestTrue(TEXT("Second group completes the physical contract"), State.NotifyStructureBroken(2));
	TestEqual(TEXT("Both shell groups enter overload/core window"), State.GetPhase(), EVectorPhysicsBossPhase::Overload);
	TestTrue(TEXT("Overload pursuit is faster than exposed pursuit"),
		State.GetPursuitSpeedCmPerSecond() > ExposedPursuitSpeed);
	TestEqual(TEXT("Only two structure transitions were recorded"), State.GetTransitionCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsBossHealthPhasesTest,
	"Vector.Boss.State.HealthOnlyOwnsDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsBossHealthPhasesTest::RunTest(const FString& Parameters)
{
	FVectorPhysicsBossState State;
	TestFalse(TEXT("65 percent cannot expose shell"), State.ApplyHealthRatio(0.65));
	TestFalse(TEXT("30 percent cannot enter overload"), State.ApplyHealthRatio(0.30));
	TestEqual(TEXT("Boss remains anchored without structure events"), State.GetPhase(), EVectorPhysicsBossPhase::AnchoredShell);
	TestTrue(TEXT("Zero health defeats"), State.ApplyHealthRatio(0.0));
	TestFalse(TEXT("Defeat is terminal"), State.NotifyStaggered());
	TestEqual(TEXT("Only death transition counted"), State.GetTransitionCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsBossPhaseOutputTest,
	"Vector.Boss.State.PhaseOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsBossPhaseOutputTest::RunTest(const FString& Parameters)
{
	FVectorPhysicsBossState State;
	const double AnchoredRam = State.GetRamIntervalSeconds();
	const double AnchoredRecovery = State.GetRecoverySeconds();
	TestEqual(TEXT("Anchored phase opens with reusable physical ammo"),
		State.SelectAttack(0, true), EVectorPhysicsBossAttack::AmmoLaunch);
	State.NotifyStructureBroken(1);
	TestEqual(TEXT("Exposed add cap shrinks"), State.GetMaximumConcurrentAdds(), 1);
	State.NotifyStructureBroken(2);
	TestEqual(TEXT("Overload spawns no adds"), State.GetMaximumConcurrentAdds(), 0);
	TestTrue(TEXT("Overload attacks more often"), State.GetRamIntervalSeconds() < AnchoredRam);
	TestTrue(TEXT("Overload recovery is more punishable"), State.GetRecoverySeconds() > AnchoredRecovery);
	TestFalse(TEXT("Zero add cap rejects spawn"), State.CanSpawnAdd(0));
	TestEqual(TEXT("Overload begins by weaponizing a live add"),
		State.SelectAttack(0, true), EVectorPhysicsBossAttack::AmmoLaunch);
	TestEqual(TEXT("Missing ammo falls back to a physical aerial burst"),
		State.SelectAttack(0, false), EVectorPhysicsBossAttack::AerialBurst);
	TestEqual(TEXT("Overload pattern retains the readable ram"),
		State.SelectAttack(1, true), EVectorPhysicsBossAttack::Ram);
	TestEqual(TEXT("Overload pattern retains the landing slam"),
		State.SelectAttack(2, true), EVectorPhysicsBossAttack::Slam);
	return true;
}

#endif
