// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorHealthComponent.h"
#include "Misc/AutomationTest.h"

#include <limits>

/**
 * S05 核心生命组件（UVectorHealthComponent）Automation。
 *
 * 纯逻辑测试（无 World）：伤害扣减、致死触发、死亡后免疫、重置、非法输入安全。
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorHealthDamageTest,
	"Vector.Health.DamageAndDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorHealthDamageTest::RunTest(const FString& Parameters)
{
	UVectorHealthComponent* Health = NewObject<UVectorHealthComponent>();
	Health->MaxHealth = 100.0;

	TestEqual(TEXT("Starts at max"), Health->GetHealth(), 100.0, 1.e-6);
	TestFalse(TEXT("Not dead initially"), Health->IsDead());

	TestFalse(TEXT("30 damage not lethal"), Health->ApplyDamage(30.0));
	TestEqual(TEXT("Health reduced to 70"), Health->GetHealth(), 70.0, 1.e-6);
	TestFalse(TEXT("Still alive"), Health->IsDead());

	TestFalse(TEXT("50 more not lethal (20 left)"), Health->ApplyDamage(50.0));
	TestEqual(TEXT("Health at 20"), Health->GetHealth(), 20.0, 1.e-6);

	TestTrue(TEXT("20 lethal"), Health->ApplyDamage(20.0));
	TestEqual(TEXT("Health clamped to zero"), Health->GetHealth(), 0.0, 1.e-6);
	TestTrue(TEXT("Dead after lethal"), Health->IsDead());

	// 死亡后免疫（不重复触发、不叠加）。
	TestFalse(TEXT("Damage after death no-op"), Health->ApplyDamage(10.0));
	TestEqual(TEXT("Health stays zero"), Health->GetHealth(), 0.0, 1.e-6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorHealthInvalidInputTest,
	"Vector.Health.InvalidInputAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorHealthInvalidInputTest::RunTest(const FString& Parameters)
{
	UVectorHealthComponent* Health = NewObject<UVectorHealthComponent>();
	Health->MaxHealth = 100.0;

	// 非有限 / 非正伤害安全拒绝。
	TestFalse(TEXT("NaN damage rejected"), Health->ApplyDamage(std::numeric_limits<double>::quiet_NaN()));
	TestFalse(TEXT("Inf damage rejected"), Health->ApplyDamage(std::numeric_limits<double>::infinity()));
	TestFalse(TEXT("Zero damage rejected"), Health->ApplyDamage(0.0));
	TestFalse(TEXT("Negative damage rejected"), Health->ApplyDamage(-50.0));
	TestEqual(TEXT("Health unchanged"), Health->GetHealth(), 100.0, 1.e-6);

	// 重置回满血。
	Health->ApplyDamage(80.0);
	TestTrue(TEXT("Damage applied before reset"), Health->ApplyDamage(30.0)); // 致死
	Health->ResetHealth();
	TestEqual(TEXT("Reset back to max"), Health->GetHealth(), 100.0, 1.e-6);
	TestFalse(TEXT("Alive after reset"), Health->IsDead());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
