// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Boss/VectorPhysicsBossState.h"
#include "GameFramework/Actor.h"
#include "VectorPCGEncounterDirector.generated.h"

class AVectorEnemy;
class AVectorPhysicsBoss;
class AVectorLowFrictionZone;
class UVectorEncounterComponent;

/** Activates deterministic PCG rooms sequentially under one hunt contract. */
UCLASS()
class VECTOR_API AVectorPCGEncounterDirector : public AActor
{
	GENERATED_BODY()

public:
	AVectorPCGEncounterDirector();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Vector|PCG|Encounter")
	int32 GetGenerationSeed() const { return GenerationSeed; }

	UFUNCTION(BlueprintPure, Category = "Vector|PCG|Encounter")
	int32 GetActiveWaveNumber() const { return FMath::Clamp(NextWaveIndex, 0, 3); }

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	int32 GenerationSeed = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<FString> ModuleSequence;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<TObjectPtr<AActor>> EncounterWaveOneSpawns;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<TObjectPtr<AActor>> EncounterWaveTwoSpawns;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TObjectPtr<AActor> BossSpawnPoint;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<TObjectPtr<AActor>> BossAddSpawnPoints;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TObjectPtr<AVectorLowFrictionZone> BossOverloadFrictionZone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TSubclassOf<AVectorEnemy> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TSubclassOf<AVectorPhysicsBoss> BossClass;

private:
	UFUNCTION()
	void HandleEncounterProgress(int32 RemainingEnemies, int32 TotalEnemies);

	UFUNCTION()
	void HandleBossPhaseChanged(
		EVectorPhysicsBossPhase PreviousPhase,
		EVectorPhysicsBossPhase CurrentPhase);

	void SpawnNextWave();
	int32 SpawnEncounterWave(const TArray<TObjectPtr<AActor>>& SpawnPoints, int32 WaveIndex);
	int32 SpawnBossWave();
	AVectorEnemy* SpawnEnemyAt(AActor* SpawnPoint, int32 SpawnIndex);
	AVectorPhysicsBoss* SpawnBossAt(AActor* SpawnPoint);

	UPROPERTY(Transient)
	TObjectPtr<UVectorEncounterComponent> Encounter;

	int32 NextWaveIndex = 0;
	bool bSpawningWave = false;
};
