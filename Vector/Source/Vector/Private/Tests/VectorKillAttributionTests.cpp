// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Combat/VectorKillAttributionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 构造一个裸账本（不依赖 World/GameMode，纯计数逻辑测试）。 */
	UVectorKillAttributionComponent* MakeLedger()
	{
		return NewObject<UVectorKillAttributionComponent>();
	}
}

/** 击杀归因：单来源计数与总数。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorKillAttributionCount,
	"Vector.KillAttribution.Count",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVectorKillAttributionCount::RunTest(const FString& Parameters)
{
	UVectorKillAttributionComponent* Ledger = MakeLedger();
	TestNotNull(TEXT("账本可创建"), Ledger);

	Ledger->RecordKill(EVectorKillCause::WallCollision);
	Ledger->RecordKill(EVectorKillCause::WallCollision);
	Ledger->RecordKill(EVectorKillCause::Hammer);

	TestEqual(TEXT("撞墙击杀数=2"), Ledger->GetKillCount(EVectorKillCause::WallCollision), 2);
	TestEqual(TEXT("锤击杀数=1"), Ledger->GetKillCount(EVectorKillCause::Hammer), 1);
	TestEqual(TEXT("总数=3"), Ledger->GetTotalKills(), 3);

	// 未记录的来源计数为 0。
	TestEqual(TEXT("冲锋击杀数=0"), Ledger->GetKillCount(EVectorKillCause::ChargerRam), 0);
	TestEqual(TEXT("Boss 冲锋击杀数=0"), Ledger->GetKillCount(EVectorKillCause::BossRam), 0);
	return true;
}

/** 击杀归因：占比计算（含无击杀时返回 0）。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorKillAttributionShare,
	"Vector.KillAttribution.Share",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVectorKillAttributionShare::RunTest(const FString& Parameters)
{
	UVectorKillAttributionComponent* Ledger = MakeLedger();

	// 无击杀：占比应为 0，无主导来源。
	double Share = 1.0;
	TestFalse(TEXT("无击杀无主导来源"), Ledger->HasDominantCause(Share));
	TestEqual(TEXT("无击杀占比=0"), Share, 0.0);
	TestEqual(TEXT("Wall 占比=0"), Ledger->GetShare(EVectorKillCause::WallCollision), 0.0);

	// 3 撞墙 + 2 锤击：Wall=60%，恰在阈值（默认 0.6，> 才报警）。
	for (int32 i = 0; i < 3; ++i)
	{
		Ledger->RecordKill(EVectorKillCause::WallCollision);
	}
	for (int32 i = 0; i < 2; ++i)
	{
		Ledger->RecordKill(EVectorKillCause::Hammer);
	}
	TestEqual(TEXT("Wall 占比=0.6"), Ledger->GetShare(EVectorKillCause::WallCollision), 0.6);

	Share = 0.0;
	const bool bDominant = Ledger->HasDominantCause(Share);
	TestFalse(TEXT("恰在 60% 不报警（> 才报警）"), bDominant);

	// 再加一个撞墙：Wall=4/6≈66.7% > 60%，报警。
	Ledger->RecordKill(EVectorKillCause::WallCollision);
	Share = 0.0;
	const bool bDominantAfter = Ledger->HasDominantCause(Share);
	TestTrue(TEXT("66.7% 触发主导报警"), bDominantAfter);
	TestTrue(TEXT("主导占比 ≈ 0.667"), FMath::IsNearlyEqual(Share, 4.0 / 6.0, 1e-6));

	return true;
}

/** 击杀归因：重置清空本局统计。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorKillAttributionReset,
	"Vector.KillAttribution.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVectorKillAttributionReset::RunTest(const FString& Parameters)
{
	UVectorKillAttributionComponent* Ledger = MakeLedger();

	Ledger->RecordKill(EVectorKillCause::BodyCollision);
	Ledger->RecordKill(EVectorKillCause::BodyCollision);
	Ledger->RecordKill(EVectorKillCause::ChargerRam);
	Ledger->RecordKill(EVectorKillCause::BossRam);
	TestEqual(TEXT("重置前总数=4"), Ledger->GetTotalKills(), 4);

	Ledger->ResetLedger();
	TestEqual(TEXT("重置后总数=0"), Ledger->GetTotalKills(), 0);
	TestEqual(TEXT("重置后 Body=0"), Ledger->GetKillCount(EVectorKillCause::BodyCollision), 0);
	TestEqual(TEXT("重置后 ChargerRam=0"), Ledger->GetKillCount(EVectorKillCause::ChargerRam), 0);
	TestEqual(TEXT("重置后 BossRam=0"), Ledger->GetKillCount(EVectorKillCause::BossRam), 0);

	// 重置后仍可继续记录（本局新会话）。
	Ledger->RecordKill(EVectorKillCause::LandingShock);
	TestEqual(TEXT("重置后可复用"), Ledger->GetTotalKills(), 1);
	return true;
}

/** 击杀归因：边界输入（未知来源归入 Other 兜底，不崩溃）。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorKillAttributionRobustness,
	"Vector.KillAttribution.Robustness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVectorKillAttributionRobustness::RunTest(const FString& Parameters)
{
	UVectorKillAttributionComponent* Ledger = MakeLedger();

	// 越界枚举：不应崩溃，归入 Other 兜底。
	Ledger->RecordKill(static_cast<EVectorKillCause>(999));
	TestEqual(TEXT("越界来源计入 Other"), Ledger->GetKillCount(EVectorKillCause::Other), 1);

	// 查越界来源：返回 0 不崩溃。
	TestEqual(TEXT("越界查询返回 0"), Ledger->GetKillCount(static_cast<EVectorKillCause>(999)), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
