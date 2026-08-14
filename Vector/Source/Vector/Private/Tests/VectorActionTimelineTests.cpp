// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorActionTypes.h"
#include "Misc/AutomationTest.h"

#include <limits>

/**
 * S02 装备动作时间线账本（FVectorActionTimeline）Automation。
 *
 * 被测对象为无 World 纯逻辑结构，不依赖场景即可确定性断言；
 * 覆盖四阶段迁移、非法状态拒绝、蓄力进度、自动阶段推进与重置。
 */

// ---------------------------------------------------------------------------
// 1. Idle → Windup：只有 Idle 能进入，重复调用被拒绝
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorActionTimelineStartWindupTest,
	"Vector.Action.Timeline.StartWindup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorActionTimelineStartWindupTest::RunTest(const FString& Parameters)
{
	FVectorActionTimeline Timeline;

	TestTrue(TEXT("Idle can start windup"), Timeline.TryStartWindup());
	TestEqual(TEXT("Phase is Windup"), Timeline.Phase, EVectorActionPhase::Windup);
	TestTrue(TEXT("Busy while windup"), Timeline.IsBusy());
	TestEqual(TEXT("Charge starts at zero"), Timeline.ChargeProgress, 0.0, 1.e-6);

	TestFalse(TEXT("Second windup rejected"), Timeline.TryStartWindup());
	TestEqual(TEXT("Still Windup after reject"), Timeline.Phase, EVectorActionPhase::Windup);

	return true;
}

// ---------------------------------------------------------------------------
// 2. Windup 蓄力进度：Advance 增长并 clamp 到 1.0
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorActionTimelineChargeProgressTest,
	"Vector.Action.Timeline.ChargeProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorActionTimelineChargeProgressTest::RunTest(const FString& Parameters)
{
	FVectorActionTimeline Timeline;
	Timeline.MaxChargeSeconds = 1.0;

	TestTrue(TEXT("Start windup"), Timeline.TryStartWindup());
	Timeline.Advance(0.5);
	TestEqual(TEXT("Half charge after 0.5s"), Timeline.ChargeProgress, 0.5, 1.e-6);

	Timeline.Advance(0.25);
	TestEqual(TEXT("Three-quarter charge"), Timeline.ChargeProgress, 0.75, 1.e-6);

	Timeline.Advance(10.0);
	TestEqual(TEXT("Charge clamps to 1.0"), Timeline.ChargeProgress, 1.0, 1.e-6);

	// 非法推进安全返回。
	Timeline.Advance(-1.0);
	TestEqual(TEXT("Negative delta no-op"), Timeline.ChargeProgress, 1.0, 1.e-6);

	return true;
}

// ---------------------------------------------------------------------------
// 3. Windup → Active（释放）：仅 Windup 可释放，释放后自动 Recovery → Idle
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorActionTimelineReleaseAndRecoveryTest,
	"Vector.Action.Timeline.ReleaseAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorActionTimelineReleaseAndRecoveryTest::RunTest(const FString& Parameters)
{
	FVectorActionTimeline Timeline;
	Timeline.ActiveSeconds = 0.10;
	Timeline.RecoverySeconds = 0.40;

	// Idle 直接释放被拒绝。
	TestFalse(TEXT("Release from Idle rejected"), Timeline.TryRelease());

	TestTrue(TEXT("Start windup"), Timeline.TryStartWindup());
	Timeline.Advance(0.5);
	TestTrue(TEXT("Release from Windup ok"), Timeline.TryRelease());
	TestEqual(TEXT("Phase is Active"), Timeline.Phase, EVectorActionPhase::Active);

	// Active 期间重复释放被拒绝。
	TestFalse(TEXT("Release from Active rejected"), Timeline.TryRelease());

	// Active 计时走完 → Recovery。
	Timeline.Advance(0.05);
	TestEqual(TEXT("Still Active mid-window"), Timeline.Phase, EVectorActionPhase::Active);
	Timeline.Advance(0.06);
	TestEqual(TEXT("Auto transition to Recovery"), Timeline.Phase, EVectorActionPhase::Recovery);

	// Recovery 走完 → Idle。
	Timeline.Advance(0.41);
	TestEqual(TEXT("Auto transition to Idle"), Timeline.Phase, EVectorActionPhase::Idle);
	TestFalse(TEXT("Not busy when Idle"), Timeline.IsBusy());

	return true;
}

// ---------------------------------------------------------------------------
// 4. 取消（Windup → Idle）与 Reset
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorActionTimelineCancelAndResetTest,
	"Vector.Action.Timeline.CancelAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorActionTimelineCancelAndResetTest::RunTest(const FString& Parameters)
{
	FVectorActionTimeline Timeline;

	// 非 Windup 取消被拒绝。
	TestFalse(TEXT("Cancel from Idle rejected"), Timeline.TryCancel());

	TestTrue(TEXT("Start windup"), Timeline.TryStartWindup());
	Timeline.Advance(0.5);
	TestTrue(TEXT("Cancel windup ok"), Timeline.TryCancel());
	TestEqual(TEXT("Back to Idle"), Timeline.Phase, EVectorActionPhase::Idle);
	TestEqual(TEXT("Charge cleared on cancel"), Timeline.ChargeProgress, 0.0, 1.e-6);

	// Reset 从任意状态回 Idle。
	TestTrue(TEXT("Start windup again"), Timeline.TryStartWindup());
	Timeline.Advance(0.9);
	Timeline.Reset();
	TestEqual(TEXT("Reset to Idle"), Timeline.Phase, EVectorActionPhase::Idle);
	TestEqual(TEXT("Reset clears charge"), Timeline.ChargeProgress, 0.0, 1.e-6);

	return true;
}

// ---------------------------------------------------------------------------
// 5. 输入维持型 Active：不自动结束，释放后 Recovery → Idle
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorActionTimelineContinuousActiveTest,
	"Vector.Action.Timeline.ContinuousActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorActionTimelineContinuousActiveTest::RunTest(const FString& Parameters)
{
	FVectorActionTimeline FiniteTimeline;
	FiniteTimeline.ActiveSeconds = 0.10;
	TestTrue(TEXT("Idle can start finite Active"), FiniteTimeline.TryStartActive());
	FiniteTimeline.Advance(0.11);
	TestEqual(TEXT("Finite Active automatically enters Recovery"),
		FiniteTimeline.Phase, EVectorActionPhase::Recovery);

	FVectorActionTimeline HitchTimeline;
	HitchTimeline.ActiveSeconds = 0.10;
	HitchTimeline.RecoverySeconds = 0.40;
	TestTrue(TEXT("Start finite action before hitch"), HitchTimeline.TryStartActive());
	HitchTimeline.Advance(0.55);
	TestEqual(TEXT("One long frame consumes Active and Recovery"),
		HitchTimeline.Phase, EVectorActionPhase::Idle);

	FVectorActionTimeline InvalidDurationTimeline;
	InvalidDurationTimeline.ActiveSeconds = std::numeric_limits<double>::infinity();
	InvalidDurationTimeline.RecoverySeconds = std::numeric_limits<double>::quiet_NaN();
	TestTrue(TEXT("Invalid durations still start safely"), InvalidDurationTimeline.TryStartActive());
	InvalidDurationTimeline.Advance(0.01);
	TestEqual(TEXT("Invalid durations cannot strand the action lock"),
		InvalidDurationTimeline.Phase, EVectorActionPhase::Idle);

	FVectorActionTimeline Timeline;
	Timeline.bActiveUntilReleased = true;
	Timeline.RecoverySeconds = 0.30;

	TestTrue(TEXT("Idle can start continuous Active"), Timeline.TryStartActive());
	Timeline.Advance(10.0);
	TestEqual(TEXT("Continuous Active does not time out"), Timeline.Phase, EVectorActionPhase::Active);
	TestTrue(TEXT("Input release enters Recovery"), Timeline.TryEndActive());
	TestEqual(TEXT("Phase is Recovery"), Timeline.Phase, EVectorActionPhase::Recovery);
	Timeline.Advance(0.31);
	TestEqual(TEXT("Recovery returns to Idle"), Timeline.Phase, EVectorActionPhase::Idle);
	TestEqual(TEXT("Idle clears continuous action progress"), Timeline.ChargeProgress, 0.0, 1.e-6);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
