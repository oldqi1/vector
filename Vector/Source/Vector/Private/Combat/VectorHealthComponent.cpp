// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorHealthComponent.h"

UVectorHealthComponent::UVectorHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UVectorHealthComponent::ApplyDamage(const double Amount)
{
	if (bDead || !FMath::IsFinite(Amount) || Amount <= 0.0)
	{
		return false;
	}

	CurrentHealth = FMath::Max(0.0, CurrentHealth - Amount);
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
	CurrentHealth = FMath::Max(1.0, MaxHealth);
	bDead = false;
}
