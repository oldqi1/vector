// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorKillAttributionTypes.h"
#include "VectorKillAttributionComponent.generated.h"

/**
 * 击杀归因账本（P0：验收 #6 + 死亡检查点监控）。
 *
 * 挂载在 GameMode 上，收集本局全部敌人击杀的来源占比：
 *   - 冲量锤直接锤死（Hammer）
 *   - 撞墙自伤（WallCollision）
 *   - 撞怪（BodyCollision）
 *   - 冲锋怪撞死他人（ChargerRam）
 *   - 落地震荡（LandingShock）
 * 局末（EndPlay / 显式 LogSummary）输出汇总，任何单一来源占比超过
 * DominanceThreshold（默认 60%）时报警——对应设计案"死亡检查点"：
 * 最优策略退化为单一手段时必须暂停并排查。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorKillAttributionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorKillAttributionComponent();

	/** 记录一次击杀（按来源计数；越界来源归入 Other 兜底）。 */
	void RecordKill(EVectorKillCause Cause);

	/** 某来源击杀数。 */
	int32 GetKillCount(EVectorKillCause Cause) const;

	/** 全部击杀总数。 */
	int32 GetTotalKills() const;

	/** 某来源占比 0~1（无击杀时返回 0）。 */
	double GetShare(EVectorKillCause Cause) const;

	/** 是否存在占比超过阈值的单一主导来源。 */
	bool HasDominantCause(double& OutShare) const;

	/** 输出本局汇总（含主导来源报警），用于局末与验收核对。 */
	void LogSummary() const;

	/** 清空本局统计（重开一局/新 PIE 会话时由 GameMode 调用）。 */
	void ResetLedger();

	/** 单一来源占比报警阈值（0~1，默认 0.6 = 60%）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|KillAttribution", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DominanceThreshold = 0.6;

private:
	/** 各来源击杀计数（索引 = EVectorKillCause）。 */
	TArray<int32> KillCounts;
};
