// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorContractExit.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/** Placeable greybox contract gate. It blocks pawns until the encounter is cleared. */
UCLASS()
class VECTOR_API AVectorContractExit : public AActor
{
	GENERATED_BODY()

public:
	AVectorContractExit();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Vector|Hunt")
	bool IsUnlocked() const { return bUnlocked; }

private:
	void BindToEncounter();

	UFUNCTION()
	void UnlockExit();

	UFUNCTION()
	void HandleEncounterProgress(int32 RemainingEnemies, int32 TotalEnemies);

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UPointLightComponent> StatusLight;

	bool bUnlocked = false;
	bool bLockedStateLogged = false;
};
