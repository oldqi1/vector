// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorHuntProgressComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FVectorOrganCountChangedSignature,
	int32, NewTotal,
	int32, Delta);

/** Run-level greybox hunt ledger. Permanent crafting remains outside prototype scope. */
UCLASS(ClassGroup = (Hunt), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorHuntProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorHuntProgressComponent();

	/** Adds a positive organ amount and returns the new total. Invalid amounts are ignored. */
	int32 CollectOrgans(int32 Amount = 1);

	void ResetProgress();

	UFUNCTION(BlueprintPure, Category = "Vector|Hunt")
	int32 GetCollectedOrgans() const { return CollectedOrgans; }

	UPROPERTY(BlueprintAssignable, Category = "Vector|Hunt")
	FVectorOrganCountChangedSignature OnOrganCountChanged;

private:
	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	int32 CollectedOrgans = 0;
};
