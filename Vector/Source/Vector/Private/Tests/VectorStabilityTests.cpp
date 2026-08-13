// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Impact/VectorImpactMath.h"
#include "Stability/VectorStabilityTypes.h"
#include "Misc/AutomationTest.h"

#include <limits>

/**
 * S01 稳定/失衡核心逻辑 Automation。
 *
 * 仿照 Morphorbit Private/Tests/ 模式：被测对象为无 World 纯逻辑
 * （FVectorImpactMath / FVectorStabilityLedger），不依赖场景即可确定性断言。
 * 测试注册用 IMPLEMENT_SIMPLE_AUTOMATION_TEST，函数体在宏外定义 RunTest。
 */

namespace VectorStabilityTests
{
	/** 构造一个已归零并触发失衡的账本（稳定度已重置、状态为 Unbalanced）。 */
	FVectorStabilityLedger MakeStaggeredLedger()
	{
		FVectorStabilityLedger Ledger;
		// 基础 30 × 弱点 1.5 = 45；两次打击 60→15→-30，第二次穿过零触发。
		Ledger.ReceiveImpactHit(30.0, 1.0, 1.0);
		const FVectorStabilityLedger::FHitResult Hit = Ledger.ReceiveImpactHit(30.0, 1.0, 1.0);
		check(Hit.bAccepted && Hit.bTriggeredStagger);
		return Ledger;
	}
}

// ---------------------------------------------------------------------------
// 1. ComputeStaggerDamage 边界：0 / 线性中点 / 全速阈值 / 全速以上 / 负值 / NaN
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorImpactStaggerDamageBoundaryTest,
	"Vector.Impact.ComputeStaggerDamage.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorImpactStaggerDamageBoundaryTest::RunTest(const FString& Parameters)
{
	// 0 cm/s → 基础 20。
	TestEqual(TEXT("Zero speed maps to base 20"), FVectorImpactMath::ComputeStaggerDamage(0.0), 20.0, 1.e-6);

	// 线性中点 145 cm/s → 20 + 10 * (145/300)。
	const double Mid = FVectorImpactMath::ComputeStaggerDamage(145.0);
	TestEqual(TEXT("Mid speed maps linearly"), Mid, 20.0 + 10.0 * (145.0 / 300.0), 1.e-6);

	// 全速阈值 290 cm/s → 封顶 30。
	TestEqual(TEXT("Full-speed threshold maps to 30"), FVectorImpactMath::ComputeStaggerDamage(290.0), 30.0, 1.e-6);

	// 超过阈值（300、1000）仍封顶 30。
	TestEqual(TEXT("Above threshold clamps to 30"), FVectorImpactMath::ComputeStaggerDamage(300.0), 30.0, 1.e-6);
	TestEqual(TEXT("Far above threshold clamps to 30"), FVectorImpactMath::ComputeStaggerDamage(1000.0), 30.0, 1.e-6);

	// 负值按 0 处理 → 20。
	TestEqual(TEXT("Negative speed treated as zero"), FVectorImpactMath::ComputeStaggerDamage(-120.0), 20.0, 1.e-6);

	// NaN 安全返回基础 20。
	TestEqual(TEXT("NaN speed treated as zero"), FVectorImpactMath::ComputeStaggerDamage(std::numeric_limits<double>::quiet_NaN()), 20.0, 1.e-6);

	return true;
}

// ---------------------------------------------------------------------------
// 2. ComputeClosingSpeed 平面化：水平面闭合速度，垂直分量被忽略
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorImpactClosingSpeedPlanarTest,
	"Vector.Impact.ComputeClosingSpeed.Planar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorImpactClosingSpeedPlanarTest::RunTest(const FString& Parameters)
{
	const FVector PlayerLocation(0.0, 0.0, 0.0);
	const FVector ImpactPoint(100.0, 0.0, 0.0);

	// 玩家沿 +X 追向静止目标：闭合速度 = 玩家水平速度 240 cm/s。
	const double Closing = FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
		PlayerLocation,
		FVector(240.0, 0.0, 0.0),
		FVector::ZeroVector,
		ImpactPoint);
	TestEqual(TEXT("Planar closing speed equals horizontal relative speed"), Closing, 240.0, 1.e-6);

	// 垂直分量（+Z）不参与闭合速度：玩家速度含 Z 分量，水平闭合仍 240。
	const double ClosingWithVertical = FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
		PlayerLocation,
		FVector(240.0, 0.0, 500.0),
		FVector::ZeroVector,
		ImpactPoint);
	TestEqual(TEXT("Vertical velocity component is ignored"), ClosingWithVertical, 240.0, 1.e-6);

	// 目标同向同速：闭合速度 = 0。
	const double ClosingSameSpeed = FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
		PlayerLocation,
		FVector(240.0, 0.0, 0.0),
		FVector(240.0, 0.0, 0.0),
		ImpactPoint);
	TestEqual(TEXT("Matching target speed yields zero closing"), ClosingSameSpeed, 0.0, 1.e-6);

	// 目标迎向玩家（-X 240）：闭合速度 = 480 → 截断到 300。
	const double ClosingHeadOn = FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
		PlayerLocation,
		FVector(240.0, 0.0, 0.0),
		FVector(-240.0, 0.0, 0.0),
		ImpactPoint);
	TestEqual(TEXT("Head-on closing clamps to 300"), ClosingHeadOn, 300.0, 1.e-6);

	// 接触点在玩家身后（-X）：方向为远离，闭合速度 = 0（非闭合输入返回零）。
	const double ClosingAway = FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
		PlayerLocation,
		FVector(240.0, 0.0, 0.0),
		FVector::ZeroVector,
		FVector(-100.0, 0.0, 0.0));
	TestEqual(TEXT("Away impact yields zero"), ClosingAway, 0.0, 1.e-6);

	// NaN 输入安全返回零。
	const double ClosingNaN = FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
		PlayerLocation,
		FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0),
		FVector::ZeroVector,
		ImpactPoint);
	TestEqual(TEXT("NaN input yields zero"), ClosingNaN, 0.0, 1.e-6);

	return true;
}

// ---------------------------------------------------------------------------
// 3. 稳定度扣减与归零触发（A1 账本）
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorStabilityLedgerDeductionTest,
	"Vector.Stability.Ledger.DeductionAndTrigger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorStabilityLedgerDeductionTest::RunTest(const FString& Parameters)
{
	FVectorStabilityLedger Ledger;

	// 初始：稳定度 60，状态 Stable。
	TestEqual(TEXT("Initial stability is maximum"), Ledger.Stability, 60.0, 1.e-6);
	TestEqual(TEXT("Initial state is Stable"), Ledger.State, EVectorStabilityState::Stable);
	TestFalse(TEXT("Initial not staggered"), Ledger.IsStaggered());

	// 基础 20 × 弱点 1.5 = 30 伤害，未归零。
	const FVectorStabilityLedger::FHitResult First = Ledger.ReceiveImpactHit(20.0, 1.0, 1.0);
	TestTrue(TEXT("First hit accepted"), First.bAccepted);
	TestEqual(TEXT("Applied damage 20*1.5=30"), First.AppliedStabilityDamage, 30.0, 1.e-6);
	TestFalse(TEXT("First hit does not trigger"), First.bTriggeredStagger);
	TestEqual(TEXT("Stability reduced to 30"), Ledger.Stability, 30.0, 1.e-6);

	// 第二次同样 30 伤害 → 稳定度穿过零 → 触发失衡并重置到 60。
	const FVectorStabilityLedger::FHitResult Second = Ledger.ReceiveImpactHit(20.0, 1.0, 1.0);
	TestTrue(TEXT("Second hit triggers stagger"), Second.bTriggeredStagger);
	TestEqual(TEXT("Stability reset to maximum on trigger"), Ledger.Stability, 60.0, 1.e-6);
	TestEqual(TEXT("State enters Unbalanced"), Ledger.State, EVectorStabilityState::Unbalanced);
	TestTrue(TEXT("IsStaggered after trigger"), Ledger.IsStaggered());

	// 基础失衡量收敛：50 收敛到 30 再 ×1.5 = 45；5 收敛到 20 再 ×1.5 = 30。
	FVectorStabilityLedger ClampLedgerHigh;
	const FVectorStabilityLedger::FHitResult ClampedHigh = ClampLedgerHigh.ReceiveImpactHit(50.0, 1.0, 1.0);
	TestEqual(TEXT("Base damage clamps to 30 then *1.5 = 45"), ClampedHigh.AppliedStabilityDamage, 45.0, 1.e-6);
	FVectorStabilityLedger ClampLedgerLow;
	const FVectorStabilityLedger::FHitResult ClampedLow = ClampLedgerLow.ReceiveImpactHit(5.0, 1.0, 1.0);
	TestEqual(TEXT("Base damage clamps to 20 then *1.5 = 30"), ClampedLow.AppliedStabilityDamage, 30.0, 1.e-6);

	// NaN 输入拒绝。
	FVectorStabilityLedger NaNGuardLedger;
	const FVectorStabilityLedger::FHitResult NaNResult =
		NaNGuardLedger.ReceiveImpactHit(std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0);
	TestFalse(TEXT("NaN hit rejected"), NaNResult.bAccepted);
	TestEqual(TEXT("NaN hit leaves stability unchanged"), NaNGuardLedger.Stability, 60.0, 1.e-6);

	return true;
}

// ---------------------------------------------------------------------------
// 4. 失衡 → 倒地 → 起身 → 稳定 状态迁移
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorStabilityStateTransitionTest,
	"Vector.Stability.State.Transition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorStabilityStateTransitionTest::RunTest(const FString& Parameters)
{
	FVectorStabilityLedger Ledger = VectorStabilityTests::MakeStaggeredLedger();
	TestEqual(TEXT("Triggered state is Unbalanced"), Ledger.State, EVectorStabilityState::Unbalanced);

	// 失衡硬直 0.20s 内不迁移。
	Ledger.AdvanceState(0.10);
	TestEqual(TEXT("Still Unbalanced before timer elapses"), Ledger.State, EVectorStabilityState::Unbalanced);

	// 跨过 0.20s → 倒地。
	Ledger.AdvanceState(0.10);
	TestEqual(TEXT("Unbalanced elapses into Downed"), Ledger.State, EVectorStabilityState::Downed);
	TestEqual(TEXT("Downed duration starts at 1.60"), Ledger.StateSecondsRemaining, 1.60, 1.e-6);

	// 倒地 1.60s → 起身。
	Ledger.AdvanceState(1.60);
	TestEqual(TEXT("Downed elapses into Rising"), Ledger.State, EVectorStabilityState::Rising);
	TestEqual(TEXT("Rising duration starts at 0.40"), Ledger.StateSecondsRemaining, 0.40, 1.e-6);

	// 起身 0.40s → 稳定。
	Ledger.AdvanceState(0.40);
	TestEqual(TEXT("Rising elapses into Stable"), Ledger.State, EVectorStabilityState::Stable);
	TestFalse(TEXT("No longer staggered after recovery"), Ledger.IsStaggered());

	// 长帧跨越多个状态：从 Unbalanced 到 Stable 总时长 = 0.20 + 1.60 + 0.40 = 2.20s；
	// 一次推进 2.5s（含浮点余量）应一步到 Stable。
	FVectorStabilityLedger LongFrameLedger = VectorStabilityTests::MakeStaggeredLedger();
	LongFrameLedger.AdvanceState(2.5);
	TestEqual(TEXT("Long frame advances to Stable"), LongFrameLedger.State, EVectorStabilityState::Stable);

	// Stable 态推进无副作用。
	FVectorStabilityLedger StableLedger;
	StableLedger.AdvanceState(1.0);
	TestEqual(TEXT("Advancing Stable is a no-op"), StableLedger.State, EVectorStabilityState::Stable);

	// 非有限帧安全返回。
	FVectorStabilityLedger NaNFrameLedger = VectorStabilityTests::MakeStaggeredLedger();
	NaNFrameLedger.AdvanceState(std::numeric_limits<double>::quiet_NaN());
	TestEqual(TEXT("NaN frame is a no-op"), NaNFrameLedger.State, EVectorStabilityState::Unbalanced);

	return true;
}

// ---------------------------------------------------------------------------
// 5. 质量/碰撞类型系数接口（基线公式：相对速度 × 质量 × 碰撞类型）
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorStabilityMultiplierTest,
	"Vector.Stability.Ledger.MultiplierPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorStabilityMultiplierTest::RunTest(const FString& Parameters)
{
	// 基础 20 × 弱点 1.5 = 30；重质量 2.0 × 撞墙 3.0 → 30 × 6 = 180。
	FVectorStabilityLedger Ledger;
	const FVectorStabilityLedger::FHitResult Hit = Ledger.ReceiveImpactHit(20.0, 2.0, 3.0);
	TestEqual(TEXT("Damage = clamp(20) * 1.5 * mass 2.0 * type 3.0 = 180"),
		Hit.AppliedStabilityDamage, 180.0, 1.e-6);
	TestTrue(TEXT("Multiplied damage triggers stagger"), Hit.bTriggeredStagger);

	// 中性乘数保持基础档（原型期默认 1.0 收敛的可验证形态）。
	FVectorStabilityLedger NeutralLedger;
	const FVectorStabilityLedger::FHitResult NeutralHit = NeutralLedger.ReceiveImpactHit(20.0, 1.0, 1.0);
	TestEqual(TEXT("Neutral multipliers keep 1.5x weakness"), NeutralHit.AppliedStabilityDamage, 30.0, 1.e-6);

	return true;
}

// ---------------------------------------------------------------------------
// 6. Reset：恢复首局账本
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorStabilityResetTest,
	"Vector.Stability.Ledger.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorStabilityResetTest::RunTest(const FString& Parameters)
{
	FVectorStabilityLedger Ledger = VectorStabilityTests::MakeStaggeredLedger();
	Ledger.Reset();

	TestEqual(TEXT("Reset restores maximum stability"), Ledger.Stability, 60.0, 1.e-6);
	TestEqual(TEXT("Reset restores Stable state"), Ledger.State, EVectorStabilityState::Stable);
	TestEqual(TEXT("Reset clears timer"), Ledger.StateSecondsRemaining, 0.0, 1.e-6);
	TestFalse(TEXT("Reset clears staggered flag"), Ledger.IsStaggered());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
