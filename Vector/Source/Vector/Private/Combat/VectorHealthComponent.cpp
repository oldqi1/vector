// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorHealthComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorHealth, Log, All);

UVectorHealthComponent::UVectorHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(1.0, MaxHealth);
	bDead = false;
}

bool UVectorHealthComponent::ApplyDamage(const double Amount)
{
	if (bDead || !FMath::IsFinite(Amount) || Amount <= 0.0)
	{
		return false;
	}

	const double PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0, CurrentHealth - Amount);
	const double AppliedDelta = CurrentHealth - PreviousHealth;
	OnHealthChanged.Broadcast(CurrentHealth, FMath::Max(1.0, MaxHealth), AppliedDelta);
	UE_LOG(LogVectorHealth, Log, TEXT("Health changed: actor=%s %.1f -> %.1f / %.1f delta=%.1f"),
		*GetNameSafe(GetOwner()),
		PreviousHealth,
		CurrentHealth,
		FMath::Max(1.0, MaxHealth),
		AppliedDelta);
	if (CurrentHealth <= 0.0)
	{
		bDead = true;
		OnDeath.Broadcast();
		return true;
	}
	return false;
}

void UVectorHealthComponent::ResetHealth()
{
	const double PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(1.0, MaxHealth);
	bDead = false;
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth, CurrentHealth - PreviousHealth);
	UE_LOG(LogVectorHealth, Log, TEXT("Health reset: actor=%s %.1f -> %.1f / %.1f"),
		*GetNameSafe(GetOwner()), PreviousHealth, CurrentHealth, CurrentHealth);
}
