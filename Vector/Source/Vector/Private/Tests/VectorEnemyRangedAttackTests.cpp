// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorEnemyRangedAttackComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorEnemyRangedVolleyGrammarTest,
	"Vector.Combat.EnemyRanged.VolleyGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorEnemyRangedVolleyGrammarTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Disabled pattern owns no projectile"),
		FVectorEnemyRangedPatternMath::GetProjectileCount(
			EVectorEnemyRangedPattern::None, 0), 0);
	TestEqual(TEXT("Arc shell always owns one readable projectile"),
		FVectorEnemyRangedPatternMath::GetProjectileCount(
			EVectorEnemyRangedPattern::ArcWeakHoming, 9), 1);
	TestEqual(TEXT("Corrosion opens with one lane"),
		FVectorEnemyRangedPatternMath::GetProjectileCount(
			EVectorEnemyRangedPattern::CorrosionVolley, 0), 1);
	TestEqual(TEXT("Corrosion alternates to three lanes"),
		FVectorEnemyRangedPatternMath::GetProjectileCount(
			EVectorEnemyRangedPattern::CorrosionVolley, 1), 3);
	TestEqual(TEXT("Corrosion returns to one lane"),
		FVectorEnemyRangedPatternMath::GetProjectileCount(
			EVectorEnemyRangedPattern::CorrosionVolley, 2), 1);

	TestEqual(TEXT("Single projectile is centered"),
		FVectorEnemyRangedPatternMath::GetSpreadAngleDegrees(0, 1, 14.0), 0.0);
	TestEqual(TEXT("Three-lane left edge"),
		FVectorEnemyRangedPatternMath::GetSpreadAngleDegrees(0, 3, 14.0), -14.0);
	TestEqual(TEXT("Three-lane center"),
		FVectorEnemyRangedPatternMath::GetSpreadAngleDegrees(1, 3, 14.0), 0.0);
	TestEqual(TEXT("Three-lane right edge"),
		FVectorEnemyRangedPatternMath::GetSpreadAngleDegrees(2, 3, 14.0), 14.0);
	return true;
}

#endif
