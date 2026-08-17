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
#include "NavigationSystem.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorPCGEncounter, Log, All);

namespace
{
	bool IsSupportedEncounterModule(const FString& ModuleName)
	{
		return ModuleName == TEXT("OpenBowl")
			|| ModuleName == TEXT("HardLane")
			|| ModuleName == TEXT("HeightShelf")
			|| ModuleName == TEXT("SlickCross");
	}

	EVectorEnemyArchetype SelectModuleArchetype(
		const FString& ModuleName,
		const int32 SpawnIndex)
	{
		// Slot order is authored with the geometry. Launchers occupy the layer
		// that exposes this module's physical recipe; rooms do not share one list.
		if (ModuleName == TEXT("HeightShelf"))
		{
			return SpawnIndex == 0 ? EVectorEnemyArchetype::HeavyRhinoBeetle
				: SpawnIndex == 1 ? EVectorEnemyArchetype::ChargerRammer
				: EVectorEnemyArchetype::LightHoppper;
		}
		if (ModuleName == TEXT("HardLane"))
		{
			return SpawnIndex == 0 ? EVectorEnemyArchetype::ChargerRammer
				: SpawnIndex == 1 ? EVectorEnemyArchetype::HeavyRhinoBeetle
				: EVectorEnemyArchetype::LightHoppper;
		}
		if (ModuleName == TEXT("SlickCross"))
		{
			return SpawnIndex == 0 ? EVectorEnemyArchetype::HeavyRhinoBeetle
				: SpawnIndex == 4 ? EVectorEnemyArchetype::ChargerRammer
				: EVectorEnemyArchetype::LightHoppper;
		}
		return SpawnIndex == 0 ? EVectorEnemyArchetype::HeavyRhinoBeetle
			: SpawnIndex == 1 ? EVectorEnemyArchetype::ChargerRammer
			: EVectorEnemyArchetype::LightHoppper;
	}

	const TCHAR* ArchetypeToLogText(const EVectorEnemyArchetype Archetype)
	{
		switch (Archetype)
		{
		case EVectorEnemyArchetype::HeavyRhinoBeetle: return TEXT("HeavyRhinoBeetle");
		case EVectorEnemyArchetype::ChargerRammer: return TEXT("ChargerRammer");
		case EVectorEnemyArchetype::LightHoppper:
		default: return TEXT("LightHopper");
		}
	}
}

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
	FString ConfigurationFailure;
	if (!ValidateRouteConfiguration(ConfigurationFailure))
	{
		UE_LOG(LogVectorPCGEncounter, Error,
			TEXT("PCG encounter configuration rejected: actor=%s reason=%s"),
			*GetName(), *ConfigurationFailure);
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

bool AVectorPCGEncounterDirector::ValidateRouteConfiguration(
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!EnemyClass || !BossClass)
	{
		OutFailureReason = TEXT("enemy or Boss class missing");
		return false;
	}
	if (!bRequireFormalRouteConfiguration)
	{
		return true;
	}
	if (EncounterWaveOneSpawns.Num() != 8 || EncounterWaveTwoSpawns.Num() != 8)
	{
		OutFailureReason = FString::Printf(
			TEXT("regular wave budget must be 8/8, found %d/%d"),
			EncounterWaveOneSpawns.Num(), EncounterWaveTwoSpawns.Num());
		return false;
	}
	for (const AActor* SpawnPoint : EncounterWaveOneSpawns)
	{
		if (!SpawnPoint)
		{
			OutFailureReason = TEXT("wave one contains null spawn point");
			return false;
		}
	}
	for (const AActor* SpawnPoint : EncounterWaveTwoSpawns)
	{
		if (!SpawnPoint)
		{
			OutFailureReason = TEXT("wave two contains null spawn point");
			return false;
		}
	}
	if (!BossSpawnPoint || BossAddSpawnPoints.Num() != 2
		|| !BossAddSpawnPoints[0] || !BossAddSpawnPoints[1])
	{
		OutFailureReason = TEXT("Boss wave requires one Boss and two add spawn points");
		return false;
	}
	if (!WaveOneExitGate || !WaveTwoExitGate)
	{
		OutFailureReason = TEXT("two sequential wave gates are required");
		return false;
	}
	if (RoomActivationTriggers.Num() != 3)
	{
		OutFailureReason = FString::Printf(
			TEXT("three room activation triggers required, found %d"),
			RoomActivationTriggers.Num());
		return false;
	}
	bool bRoomIndices[3] = { false, false, false };
	for (const AVectorPCGRoomTrigger* Trigger : RoomActivationTriggers)
	{
		if (!Trigger || Trigger->RoomIndex < 0 || Trigger->RoomIndex >= 3
			|| bRoomIndices[Trigger->RoomIndex])
		{
			OutFailureReason = TEXT("room trigger indices must be unique 0/1/2");
			return false;
		}
		bRoomIndices[Trigger->RoomIndex] = true;
	}
	if (!BossOverloadFrictionZone)
	{
		OutFailureReason = TEXT("Boss overload friction zone missing");
		return false;
	}
	if (ModuleSequence.Num() != 5
		|| ModuleSequence[0] != TEXT("SafeStart")
		|| ModuleSequence[3] != TEXT("BossRing")
		|| ModuleSequence[4] != TEXT("Extraction"))
	{
		OutFailureReason = TEXT("module sequence must be SafeStart>room>room>BossRing>Extraction");
		return false;
	}
	if (!IsSupportedEncounterModule(ModuleSequence[1])
		|| !IsSupportedEncounterModule(ModuleSequence[2]))
	{
		OutFailureReason = FString::Printf(
			TEXT("encounter modules must own a supported geometry/enemy recipe, found %s/%s"),
			*ModuleSequence[1], *ModuleSequence[2]);
		return false;
	}
	return true;
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
		UE_LOG(LogVectorPCGEncounter, Log,
			TEXT("PCG room exit verified: wave=1 gate=%s locked=%s check=%s"),
			*WaveOneExitGate->GetName(),
			WaveOneExitGate->IsGateLocked() ? TEXT("YES") : TEXT("no"),
			WaveOneExitGate->IsGateLocked() ? TEXT("FAIL") : TEXT("PASS"));
	}
	else if (ClearedWaveIndex == 1 && WaveTwoExitGate)
	{
		WaveTwoExitGate->SetGateLocked(false);
		UE_LOG(LogVectorPCGEncounter, Log,
			TEXT("PCG room exit verified: wave=2 gate=%s locked=%s check=%s"),
			*WaveTwoExitGate->GetName(),
			WaveTwoExitGate->IsGateLocked() ? TEXT("YES") : TEXT("no"),
			WaveTwoExitGate->IsGateLocked() ? TEXT("FAIL") : TEXT("PASS"));
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
		if (AVectorEnemy* Enemy = SpawnEnemyAt(SpawnPoints[Index], Index, WaveIndex))
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
		if (AVectorEnemy* Enemy = SpawnEnemyAt(BossAddSpawnPoints[Index], Index, -1))
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
	const int32 SpawnIndex,
	const int32 WaveIndex)
{
	if (!GetWorld() || !EnemyClass || !SpawnPoint)
	{
		return nullptr;
	}
	FNavLocation ProjectedLocation;
	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bProjectedToNavigation = NavigationSystem
		&& NavigationSystem->ProjectPointToNavigation(
			SpawnPoint->GetActorLocation(), ProjectedLocation,
			FVector(160.0, 160.0, 220.0));
	const FVector ResolvedSpawnLocation = bProjectedToNavigation
		? ProjectedLocation.Location + FVector(0.0, 0.0, 100.0)
		: SpawnPoint->GetActorLocation() + FVector(0.0, 0.0, 80.0);
	if (!bProjectedToNavigation)
	{
		UE_LOG(LogVectorPCGEncounter, Warning,
			TEXT("PCG enemy nav projection failed: moduleIndex=%d slot=%d marker=%s queryExtent=(160,160,220)"),
			WaveIndex + 1, SpawnIndex,
			*SpawnPoint->GetActorLocation().ToCompactString());
	}
	const FTransform SpawnTransform(SpawnPoint->GetActorRotation(),
		ResolvedSpawnLocation);
	AVectorEnemy* Enemy = GetWorld()->SpawnActorDeferred<AVectorEnemy>(
		EnemyClass, SpawnTransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}
	const int32 ModuleIndex = WaveIndex + 1;
	const FString ModuleName = WaveIndex >= 0 && ModuleSequence.IsValidIndex(ModuleIndex)
		? ModuleSequence[ModuleIndex]
		: TEXT("BossRing");
	Enemy->Archetype = WaveIndex >= 0
		? SelectModuleArchetype(ModuleName, SpawnIndex)
		: EVectorEnemyArchetype::LightHoppper;
	UGameplayStatics::FinishSpawningActor(Enemy, SpawnTransform);
	Enemy->ConfigureEncounterVoidRecovery(
		EnemyVoidRecoveryFloorWorldZ,
		ResolvedSpawnLocation);
	UE_LOG(LogVectorPCGEncounter, Log,
		TEXT("PCG enemy slot: module=%s wave=%d slot=%d markerZ=%.0f spawnZ=%.0f nav=%s archetype=%s"),
		*ModuleName, WaveIndex + 1, SpawnIndex, SpawnPoint->GetActorLocation().Z,
		ResolvedSpawnLocation.Z, bProjectedToNavigation ? TEXT("PASS") : TEXT("FAIL"),
		ArchetypeToLogText(Enemy->Archetype));
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
