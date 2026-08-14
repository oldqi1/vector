// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hunt/VectorEncounterComponent.h"

#include "Combat/VectorEnemy.h"
#include "Combat/VectorHealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEncounter, Log, All);

UVectorEncounterComponent::UVectorEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorEncounterComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UVectorEncounterComponent::RegisterExistingEnemies));
	}
}

void UVectorEncounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearEnemyBindings();
	Super::EndPlay(EndPlayReason);
}

void UVectorEncounterComponent::StartEncounter(const int32 EnemyCount)
{
	TotalEnemies = FMath::Max(0, EnemyCount);
	RemainingEnemies = TotalEnemies;
	EncounterState = TotalEnemies > 0
		? EVectorEncounterState::Active
		: EVectorEncounterState::Inactive;
	StartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	CompletionTimeSeconds = 0.0;

	UE_LOG(LogVectorEncounter, Log,
		TEXT("Encounter started: state=%s total=%d"),
		EncounterState == EVectorEncounterState::Active ? TEXT("ACTIVE") : TEXT("INACTIVE"),
		TotalEnemies);
	OnEncounterProgress.Broadcast(RemainingEnemies, TotalEnemies);
}

void UVectorEncounterComponent::NotifyEnemyDefeated()
{
	if (EncounterState != EVectorEncounterState::Active || RemainingEnemies <= 0)
	{
		UE_LOG(LogVectorEncounter, Verbose,
			TEXT("Encounter defeat ignored: state=%d remaining=%d total=%d"),
			static_cast<int32>(EncounterState), RemainingEnemies, TotalEnemies);
		return;
	}

	--RemainingEnemies;
	UE_LOG(LogVectorEncounter, Log,
		TEXT("Encounter enemy defeated: remaining=%d total=%d"),
		RemainingEnemies, TotalEnemies);
	OnEncounterProgress.Broadcast(RemainingEnemies, TotalEnemies);

	if (RemainingEnemies == 0)
	{
		EncounterState = EVectorEncounterState::Completed;
		CompletionTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : StartTimeSeconds;
		UE_LOG(LogVectorEncounter, Log,
			TEXT("Contract completed: defeated=%d elapsed=%.1fs exit=OPEN"),
			TotalEnemies, GetElapsedSeconds());
		OnEncounterCompleted.Broadcast();
	}
}

double UVectorEncounterComponent::GetElapsedSeconds() const
{
	if (EncounterState == EVectorEncounterState::Inactive)
	{
		return 0.0;
	}
	const double EndTime = EncounterState == EVectorEncounterState::Completed
		? CompletionTimeSeconds
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : StartTimeSeconds);
	return FMath::Max(0.0, EndTime - StartTimeSeconds);
}

void UVectorEncounterComponent::RegisterExistingEnemies()
{
	ClearEnemyBindings();

	UWorld* World = GetWorld();
	if (!World)
	{
		StartEncounter(0);
		return;
	}

	for (TActorIterator<AVectorEnemy> It(World); It; ++It)
	{
		AVectorEnemy* Enemy = *It;
		UVectorHealthComponent* Health = Enemy
			? Enemy->FindComponentByClass<UVectorHealthComponent>()
			: nullptr;
		if (!Health || Health->IsDead() || RegisteredHealthComponents.Contains(Health))
		{
			continue;
		}

		RegisteredHealthComponents.Add(Health);
		Health->OnDeath.AddUniqueDynamic(this, &UVectorEncounterComponent::HandleRegisteredEnemyDeath);
	}

	StartEncounter(RegisteredHealthComponents.Num());
}

void UVectorEncounterComponent::HandleRegisteredEnemyDeath()
{
	NotifyEnemyDefeated();
}

void UVectorEncounterComponent::ClearEnemyBindings()
{
	for (const TWeakObjectPtr<UVectorHealthComponent>& Health : RegisteredHealthComponents)
	{
		if (Health.IsValid())
		{
			Health->OnDeath.RemoveDynamic(this, &UVectorEncounterComponent::HandleRegisteredEnemyDeath);
		}
	}
	RegisteredHealthComponents.Empty();
}
