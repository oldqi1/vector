// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Impact/VectorImpactMath.h"
#include "Misc/AutomationTest.h"

#include <limits>

/**
 * S03 碰撞伤害公式（FVectorImpactMath::ComputeCollisionDamage）Automation。
 *
 * 纯函数测试：速度阈值截断、线性增长、质量/类型乘数、硬上限、非法输入安全。
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorCollisionDamageThresholdTest,
	"Vector.Impact.ComputeCollisionDamage.Threshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorCollisionDamageThresholdTest::RunTest(const FString& Parameters)
{
	// 低于阈值：无伤害。
	TestEqual(TEXT("Below threshold -> 0"),
		FVectorImpactMath::ComputeCollisionDamage(299.0), 0.0, 1.e-6);
	TestEqual(TEXT("At threshold -> 0"),
		FVectorImpactMath::ComputeCollisionDamage(300.0), 0.0, 1.e-6);

	// 刚过阈值：从 0 平滑增长。速度 320，超速 20，0.05/cm/s → 1.0。
	TestEqual(TEXT("Just above threshold scales linearly"),
		FVectorImpactMath::ComputeCollisionDamage(320.0), 20.0 * 0.05, 1.e-6);

	// 速度 720（满蓄冲量常见值）：(720-300)*0.05 = 21。
	TestEqual(TEXT("Full-push speed damage"),
		FVectorImpactMath::ComputeCollisionDamage(720.0), 21.0, 1.e-6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorCollisionDamageMultiplierTest,
	"Vector.Impact.ComputeCollisionDamage.Multiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorCollisionDamageMultiplierTest::RunTest(const FString& Parameters)
{
	// 速度 720 基础 21。
	const double Base = FVectorImpactMath::ComputeCollisionDamage(720.0);

	// 质量系数 0.6（中）→ 12.6；1.5（重撞墙系数）→ 31.5。
	TestEqual(TEXT("Mass multiplier scales damage"),
		FVectorImpactMath::ComputeCollisionDamage(720.0, 0.6), Base * 0.6, 1.e-6);
	TestEqual(TEXT("Collision type multiplier scales damage"),
		FVectorImpactMath::ComputeCollisionDamage(720.0, 1.0, 1.5), Base * 1.5, 1.e-6);

	// 负/非法乘数按 0 处理。
	TestEqual(TEXT("Negative mass multiplier -> 0"),
		FVectorImpactMath::ComputeCollisionDamage(720.0, -1.0), 0.0, 1.e-6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorCollisionDamageCapTest,
	"Vector.Impact.ComputeCollisionDamage.CapAndInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorCollisionDamageCapTest::RunTest(const FString& Parameters)
{
	// 硬上限：超高速被 clamp 到 MaxDamage。
	TestEqual(TEXT("Extreme speed clamped to max"),
		FVectorImpactMath::ComputeCollisionDamage(100000.0, 1.0, 1.0, 300.0, 0.05, 50.0),
		50.0, 1.e-6);

	// NaN / Inf 输入安全返回 0。
	TestEqual(TEXT("NaN speed -> 0"),
		FVectorImpactMath::ComputeCollisionDamage(std::numeric_limits<double>::quiet_NaN()),
		0.0, 1.e-6);
	TestEqual(TEXT("Inf speed -> 0"),
		FVectorImpactMath::ComputeCollisionDamage(std::numeric_limits<double>::infinity()),
		0.0, 1.e-6);

	// 自定义阈值/每速/上限。
	TestEqual(TEXT("Custom params respected"),
		FVectorImpactMath::ComputeCollisionDamage(400.0, 1.0, 1.0, 200.0, 0.1, 10.0),
		10.0, 1.e-6); // (400-200)*0.1=20 → clamp 10

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
