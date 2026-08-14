// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hunt/VectorHuntProgressComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorHunt, Log, All);

UVectorHuntProgressComponent::UVectorHuntProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UVectorHuntProgressComponent::CollectOrgans(const int32 Amount)
{
	if (bExtractionComplete)
	{
		UE_LOG(LogVectorHunt, Verbose,
			TEXT("Organ collection rejected: hunt already extracted amount=%d secured=%d"),
			Amount, SecuredOrgans);
		return CollectedOrgans;
	}
	if (Amount <= 0)
	{
		UE_LOG(LogVectorHunt, Verbose,
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
	SecuredOrgans = 0;
	bExtractionComplete = false;
	if (PreviousTotal != 0)
	{
		OnOrganCountChanged.Broadcast(0, -PreviousTotal);
	}
	UE_LOG(LogVectorHunt, Log,
		TEXT("Hunt progress reset: organs=%d -> 0"), PreviousTotal);
}

bool UVectorHuntProgressComponent::CompleteExtraction()
{
	if (bExtractionComplete)
	{
		UE_LOG(LogVectorHunt, Verbose,
			TEXT("Extraction completion ignored: already complete secured=%d"),
			SecuredOrgans);
		return false;
	}

	bExtractionComplete = true;
	SecuredOrgans = CollectedOrgans;
	UE_LOG(LogVectorHunt, Log,
		TEXT("Hunt extraction completed: organs=%d"), SecuredOrgans);
	OnExtractionCompleted.Broadcast(SecuredOrgans);
	return true;
}
