// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorEncounterComponent.generated.h"

class UVectorHealthComponent;

UENUM(BlueprintType)
enum class EVectorEncounterState : uint8
{
	Inactive,
	Active,
	Completed,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FVectorEncounterProgressSignature,
	int32, RemainingEnemies,
	int32, TotalEnemies);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVectorEncounterCompletedSignature);

/** Tracks the fixed greybox hunt contract independently from enemy presentation. */
UCLASS(ClassGroup = (Hunt), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorEncounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorEncounterComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Starts a contract with an explicit count. A non-positive count leaves it inactive. */
	void StartEncounter(int32 EnemyCount);

	/** Pure ledger transition used by bound enemy death events and Automation tests. */
	void NotifyEnemyDefeated();

	/** Scans live VectorEnemy actors and binds their health death events. */
	UFUNCTION(BlueprintCallable, Category = "Vector|Hunt")
	void RegisterExistingEnemies();

	UFUNCTION(BlueprintPure, Category = "Vector|Hunt")
	EVectorEncounterState GetEncounterState() const { return EncounterState; }

	UFUNCTION(BlueprintPure, Category = "Vector|Hunt")
	int32 GetTotalEnemies() const { return TotalEnemies; }

	UFUNCTION(BlueprintPure, Category = "Vector|Hunt")
	int32 GetRemainingEnemies() const { return RemainingEnemies; }

	UFUNCTION(BlueprintPure, Category = "Vector|Hunt")
	bool IsComplete() const { return EncounterState == EVectorEncounterState::Completed; }

	UPROPERTY(BlueprintAssignable, Category = "Vector|Hunt")
	FVectorEncounterProgressSignature OnEncounterProgress;

	UPROPERTY(BlueprintAssignable, Category = "Vector|Hunt")
	FVectorEncounterCompletedSignature OnEncounterCompleted;

private:
	UFUNCTION()
	void HandleRegisteredEnemyDeath();

	void ClearEnemyBindings();

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	EVectorEncounterState EncounterState = EVectorEncounterState::Inactive;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	int32 TotalEnemies = 0;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	int32 RemainingEnemies = 0;

	TSet<TWeakObjectPtr<UVectorHealthComponent>> RegisteredHealthComponents;
};
