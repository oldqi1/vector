// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCG/VectorPCGEncounterDirector.h"
#include "PCG/VectorPCGRoomTrigger.h"
#include "PCG/VectorPCGWaveGate.h"

#include "Boss/VectorPhysicsBoss.h"
#include "Combat/VectorEnemy.h"
#include "Engine/World.h"
#include "Environment/VectorLowFrictionZone.h"
#include "Hunt/VectorEncounterComponent.h"
#include "Kismet/GameplayStatics.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorPCGEncounter, Log, All);

AVectorPCGEncounterDirector::AVectorPCGEncounterDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	EnemyClass = AVectorEnemy::StaticClass();
	BossClass = AVectorPhysicsBoss::StaticClass();
}

void AVectorPCGEncounterDirector::BeginPlay()
{
	Super::BeginPlay();
	AVectorGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AVectorGameMode>() : nullptr;
	Encounter = GameMode ? GameMode->Encounter : nullptr;
	if (!Encounter)
	{
		UE_LOG(LogVectorPCGEncounter, Error,
			TEXT("PCG encounter director has no hunt ledger: actor=%s"), *GetName());
		return;
	}

	Encounter->BeginDynamicEncounter();
	if (WaveOneExitGate)
	{
		WaveOneExitGate->SetGateLocked(true);
	}
	if (WaveTwoExitGate)
	{
		WaveTwoExitGate->SetGateLocked(true);
	}
	Encounter->OnEncounterProgress.AddUniqueDynamic(
		this, &AVectorPCGEncounterDirector::HandleEncounterProgress);
	for (AVectorPCGRoomTrigger* Trigger : RoomActivationTriggers)
	{
		if (Trigger)
		{
			Trigger->OnRoomEntered.AddUniqueDynamic(
				this, &AVectorPCGEncounterDirector::HandleRoomEntered);
		}
	}
	if (RoomActivationTriggers.Num() == 0)
	{
		UE_LOG(LogVectorPCGEncounter, Warning,
			TEXT("PCG route has no room triggers; activating first wave immediately"));
		SpawnNextWave();
	}
}

void AVectorPCGEncounterDirector::HandleRoomEntered(const int32 RoomIndex)
{
	if (bCurrentWaveActive || bSpawningWave || RoomIndex != NextWaveIndex)
	{
		UE_LOG(LogVectorPCGEncounter, Warning,
			TEXT("PCG room activation rejected: requested=%d expected=%d active=%d spawning=%d"),
			RoomIndex, NextWaveIndex, bCurrentWaveActive ? 1 : 0, bSpawningWave ? 1 : 0);
		return;
	}
	SpawnNextWave();
}

void AVectorPCGEncounterDirector::HandleBossPhaseChanged(
	const EVectorPhysicsBossPhase PreviousPhase,
	const EVectorPhysicsBossPhase CurrentPhase)
{
	if (!BossOverloadFrictionZone)
	{
		return;
	}
	const bool bEnable = CurrentPhase == EVectorPhysicsBossPhase::Overload;
	BossOverloadFrictionZone->SetZoneActive(bEnable);
	UE_LOG(LogVectorPCGEncounter, Log,
		TEXT("PCG BossRing friction: previous=%d current=%d active=%s"),
		static_cast<int32>(PreviousPhase), static_cast<int32>(CurrentPhase),
		bEnable ? TEXT("YES") : TEXT("no"));
}

void AVectorPCGEncounterDirector::HandleEncounterProgress(
	const int32 RemainingEnemies,
	const int32 TotalEnemies)
{
	if (bSpawningWave || !bCurrentWaveActive || !Encounter
		|| Encounter->IsComplete() || RemainingEnemies != 0)
	{
		return;
	}
	bCurrentWaveActive = false;
	const int32 ClearedWaveIndex = NextWaveIndex - 1;
	if (ClearedWaveIndex == 0 && WaveOneExitGate)
	{
		WaveOneExitGate->SetGateLocked(false);
	}
	else if (ClearedWaveIndex == 1 && WaveTwoExitGate)
	{
		WaveTwoExitGate->SetGateLocked(false);
	}
	else if (ClearedWaveIndex == 2)
	{
		Encounter->SealDynamicEncounter();
	}
	UE_LOG(LogVectorPCGEncounter, Log,
		TEXT("PCG wave cleared: wave=%d nextRoom=%d cumulativeTotal=%d sealed=%s"),
		ClearedWaveIndex + 1, NextWaveIndex, TotalEnemies,
		ClearedWaveIndex == 2 ? TEXT("YES") : TEXT("no"));
	if (RoomActivationTriggers.Num() == 0 && ClearedWaveIndex < 2)
	{
		SpawnNextWave();
	}
}

void AVectorPCGEncounterDirector::SpawnNextWave()
{
	if (!Encounter || bSpawningWave)
	{
		return;
	}

	bSpawningWave = true;
	int32 SpawnedCount = 0;
	const int32 WaveBeingSpawned = NextWaveIndex++;
	switch (WaveBeingSpawned)
	{
	case 0:
		SpawnedCount = SpawnEncounterWave(EncounterWaveOneSpawns, WaveBeingSpawned);
		break;
	case 1:
		SpawnedCount = SpawnEncounterWave(EncounterWaveTwoSpawns, WaveBeingSpawned);
		break;
	case 2:
		SpawnedCount = SpawnBossWave();
		break;
	default:
		bSpawningWave = false;
		Encounter->SealDynamicEncounter();
		UE_LOG(LogVectorPCGEncounter, Log,
			TEXT("PCG encounter route sealed: total=%d"), Encounter->GetTotalEnemies());
		return;
	}
	bSpawningWave = false;
	bCurrentWaveActive = SpawnedCount > 0;

	UE_LOG(LogVectorPCGEncounter, Log,
		TEXT("PCG wave activated: wave=%d spawned=%d remaining=%d total=%d"),
		WaveBeingSpawned + 1, SpawnedCount,
		Encounter->GetRemainingEnemies(), Encounter->GetTotalEnemies());
	if (SpawnedCount == 0)
	{
		bCurrentWaveActive = true;
		HandleEncounterProgress(0, Encounter->GetTotalEnemies());
	}
}

int32 AVectorPCGEncounterDirector::SpawnEncounterWave(
	const TArray<TObjectPtr<AActor>>& SpawnPoints,
	const int32 WaveIndex)
{
	int32 SpawnedCount = 0;
	for (int32 Index = 0; Index < SpawnPoints.Num(); ++Index)
	{
		if (AVectorEnemy* Enemy = SpawnEnemyAt(SpawnPoints[Index], Index))
		{
			if (Encounter->RegisterSpawnedEnemy(Enemy))
			{
				++SpawnedCount;
			}
			else
			{
				Enemy->Destroy();
			}
		}
	}
	UE_LOG(LogVectorPCGEncounter, Log,
		TEXT("PCG regular wave built: index=%d requested=%d spawned=%d"),
		WaveIndex, SpawnPoints.Num(), SpawnedCount);
	return SpawnedCount;
}

int32 AVectorPCGEncounterDirector::SpawnBossWave()
{
	int32 SpawnedCount = 0;
	if (AVectorPhysicsBoss* Boss = SpawnBossAt(BossSpawnPoint))
	{
		Boss->OnBossPhaseChanged.AddUniqueDynamic(
			this, &AVectorPCGEncounterDirector::HandleBossPhaseChanged);
		if (Encounter->RegisterSpawnedEnemy(Boss))
		{
			++SpawnedCount;
		}
		else
		{
			Boss->Destroy();
		}
	}
	for (int32 Index = 0; Index < BossAddSpawnPoints.Num(); ++Index)
	{
		if (AVectorEnemy* Enemy = SpawnEnemyAt(BossAddSpawnPoints[Index], Index + 2))
		{
			if (Encounter->RegisterSpawnedEnemy(Enemy))
			{
				++SpawnedCount;
			}
			else
			{
				Enemy->Destroy();
			}
		}
	}
	return SpawnedCount;
}

AVectorEnemy* AVectorPCGEncounterDirector::SpawnEnemyAt(
	AActor* SpawnPoint,
	const int32 SpawnIndex)
{
	if (!GetWorld() || !EnemyClass || !SpawnPoint)
	{
		return nullptr;
	}
	const FTransform SpawnTransform(SpawnPoint->GetActorRotation(),
		SpawnPoint->GetActorLocation() + FVector(0.0, 0.0, 80.0));
	AVectorEnemy* Enemy = GetWorld()->SpawnActorDeferred<AVectorEnemy>(
		EnemyClass, SpawnTransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}
	if (SpawnIndex == 0)
	{
		Enemy->Archetype = EVectorEnemyArchetype::HeavyRhinoBeetle;
	}
	else if (SpawnIndex == 1)
	{
		Enemy->Archetype = EVectorEnemyArchetype::ChargerRammer;
	}
	else
	{
		Enemy->Archetype = EVectorEnemyArchetype::LightHoppper;
	}
	UGameplayStatics::FinishSpawningActor(Enemy, SpawnTransform);
	return Enemy;
}

AVectorPhysicsBoss* AVectorPCGEncounterDirector::SpawnBossAt(AActor* SpawnPoint)
{
	if (!GetWorld() || !BossClass || !SpawnPoint)
	{
		return nullptr;
	}
	const FTransform SpawnTransform(SpawnPoint->GetActorRotation(),
		SpawnPoint->GetActorLocation() + FVector(0.0, 0.0, 120.0));
	AVectorPhysicsBoss* Boss = GetWorld()->SpawnActorDeferred<AVectorPhysicsBoss>(
		BossClass, SpawnTransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Boss)
	{
		UGameplayStatics::FinishSpawningActor(Boss, SpawnTransform);
	}
	return Boss;
}
