// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hunt/VectorHuntProgressComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorHunt, Log, All);

UVectorHuntProgressComponent::UVectorHuntProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UVectorHuntProgressComponent::CollectOrgans(const int32 Amount)
{
	if (Amount <= 0)
	{
		UE_LOG(LogVectorHunt, Warning,
			TEXT("Organ collection rejected: amount=%d total=%d"),
			Amount, CollectedOrgans);
		return CollectedOrgans;
	}

	const int32 PreviousTotal = CollectedOrgans;
	const int32 SafeAmount = FMath::Min(Amount, MAX_int32 - CollectedOrgans);
	CollectedOrgans += SafeAmount;
	const int32 AppliedDelta = CollectedOrgans - PreviousTotal;
	OnOrganCountChanged.Broadcast(CollectedOrgans, AppliedDelta);
	UE_LOG(LogVectorHunt, Log,
		TEXT("Organ collected: amount=%d total=%d"),
		AppliedDelta, CollectedOrgans);
	return CollectedOrgans;
}

void UVectorHuntProgressComponent::ResetProgress()
{
	const int32 PreviousTotal = CollectedOrgans;
	CollectedOrgans = 0;
	if (PreviousTotal != 0)
	{
		OnOrganCountChanged.Broadcast(0, -PreviousTotal);
	}
	UE_LOG(LogVectorHunt, Log,
		TEXT("Hunt progress reset: organs=%d -> 0"), PreviousTotal);
}
