// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorKillAttributionComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorKillAttribution, Log, All);

namespace
{
	constexpr int32 CauseCount = static_cast<int32>(EVectorKillCause::Count);
}

UVectorKillAttributionComponent::UVectorKillAttributionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	KillCounts.SetNum(CauseCount);
}

void UVectorKillAttributionComponent::RecordKill(const EVectorKillCause Cause)
{
	int32 Index = static_cast<int32>(Cause);
	if (Index < 0 || Index >= CauseCount)
	{
		Index = static_cast<int32>(EVectorKillCause::Other);
	}
	KillCounts[Index]++;
}

int32 UVectorKillAttributionComponent::GetKillCount(const EVectorKillCause Cause) const
{
	const int32 Index = static_cast<int32>(Cause);
	if (Index < 0 || Index >= CauseCount)
	{
		return 0;
	}
	return KillCounts[Index];
}

int32 UVectorKillAttributionComponent::GetTotalKills() const
{
	int32 Total = 0;
	for (const int32 Count : KillCounts)
	{
		Total += Count;
	}
	return Total;
}

double UVectorKillAttributionComponent::GetShare(const EVectorKillCause Cause) const
{
	const int32 Total = GetTotalKills();
	if (Total <= 0)
	{
		return 0.0;
	}
	return static_cast<double>(GetKillCount(Cause)) / static_cast<double>(Total);
}

bool UVectorKillAttributionComponent::HasDominantCause(double& OutShare) const
{
	OutShare = 0.0;
	for (int32 Index = 0; Index < CauseCount; ++Index)
	{
		const double Share = GetShare(static_cast<EVectorKillCause>(Index));
		if (Share > OutShare)
		{
			OutShare = Share;
		}
	}
	return OutShare > DominanceThreshold;
}

void UVectorKillAttributionComponent::LogSummary() const
{
	const int32 Total = GetTotalKills();
	UE_LOG(LogVectorKillAttribution, Log, TEXT("===== Kill Attribution Summary (total=%d) ====="), Total);
	for (int32 Index = 0; Index < CauseCount; ++Index)
	{
		const EVectorKillCause Cause = static_cast<EVectorKillCause>(Index);
		UE_LOG(LogVectorKillAttribution, Log, TEXT("  %-16s %3d  (%.1f%%)"),
			*UEnum::GetValueAsString(Cause),
			GetKillCount(Cause),
			GetShare(Cause) * 100.0);
	}

	double DominantShare = 0.0;
	if (Total > 0 && HasDominantCause(DominantShare))
	{
		UE_LOG(LogVectorKillAttribution, Warning,
			TEXT("!!! DOMINANT KILL CAUSE: single source %.1f%% > threshold %.0f%% — "
				 "游戏可能退化为单一击杀手段（推墙唯一解风险），请人工核对死亡检查点！"),
			DominantShare * 100.0, DominanceThreshold * 100.0);
	}
}

void UVectorKillAttributionComponent::ResetLedger()
{
	KillCounts.SetNum(CauseCount);
	for (int32& Count : KillCounts)
	{
		Count = 0;
	}
}
