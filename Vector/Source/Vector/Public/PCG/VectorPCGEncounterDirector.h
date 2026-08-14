// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorPCGEncounterDirector.generated.h"

class AVectorEnemy;
class AVectorPhysicsBoss;
class UVectorEncounterComponent;

/** Activates deterministic PCG rooms sequentially under one hunt contract. */
UCLASS()
class VECTOR_API AVectorPCGEncounterDirector : public AActor
{
	GENERATED_BODY()

public:
	AVectorPCGEncounterDirector();
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<TObjectPtr<AActor>> EncounterWaveOneSpawns;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<TObjectPtr<AActor>> EncounterWaveTwoSpawns;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TObjectPtr<AActor> BossSpawnPoint;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TArray<TObjectPtr<AActor>> BossAddSpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TSubclassOf<AVectorEnemy> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	TSubclassOf<AVectorPhysicsBoss> BossClass;

private:
	UFUNCTION()
	void HandleEncounterProgress(int32 RemainingEnemies, int32 TotalEnemies);

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
