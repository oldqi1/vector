// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Boss/VectorPhysicsBossState.h"
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
	TestEqual(TEXT("Anchored add budget"), State.GetMaximumConcurrentAdds(), 2);
	TestTrue(TEXT("First add is allowed"), State.CanSpawnAdd(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsBossStaggerExposeTest,
	"Vector.Boss.State.StaggerExposesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsBossStaggerExposeTest::RunTest(const FString& Parameters)
{
	FVectorPhysicsBossState State;
	TestTrue(TEXT("First stagger changes phase"), State.NotifyStaggered());
	TestEqual(TEXT("Stagger exposes the shell"), State.GetPhase(), EVectorPhysicsBossPhase::ExposedShell);
	TestEqual(TEXT("Exposed mass falls through the same physical rule"), State.GetEffectivePhysicalMass(), 4.0);
	TestFalse(TEXT("Repeated stagger does not duplicate transition"), State.NotifyStaggered());
	TestEqual(TEXT("Only one transition was recorded"), State.GetTransitionCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorPhysicsBossHealthPhasesTest,
	"Vector.Boss.State.HealthTransitionsAreMonotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorPhysicsBossHealthPhasesTest::RunTest(const FString& Parameters)
{
	FVectorPhysicsBossState State;
	TestTrue(TEXT("65 percent exposes shell"), State.ApplyHealthRatio(0.65));
	TestTrue(TEXT("30 percent enters overload"), State.ApplyHealthRatio(0.30));
	TestFalse(TEXT("Healing cannot regress phase"), State.ApplyHealthRatio(0.90));
	TestEqual(TEXT("Phase remains overload"), State.GetPhase(), EVectorPhysicsBossPhase::Overload);
	TestTrue(TEXT("Zero health defeats"), State.ApplyHealthRatio(0.0));
	TestFalse(TEXT("Defeat is terminal"), State.NotifyStaggered());
	TestEqual(TEXT("All forward transitions counted"), State.GetTransitionCount(), 3);
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
	State.ApplyHealthRatio(0.60);
	TestEqual(TEXT("Exposed add cap shrinks"), State.GetMaximumConcurrentAdds(), 1);
	State.ApplyHealthRatio(0.20);
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
