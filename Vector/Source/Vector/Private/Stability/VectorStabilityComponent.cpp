// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stability/VectorStabilityComponent.h"

UVectorStabilityComponent::UVectorStabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVectorStabilityComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyConfiguration();
}

void UVectorStabilityComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ApplyConfiguration();
	Ledger.AdvanceState(DeltaTime);
}

double UVectorStabilityComponent::ReceiveImpactHit(
	const double BaseStaggerDamage,
	const EVectorMassClass InMassClass,
	const EVectorImpactType ImpactType)
{
	ApplyConfiguration();

	const double MassMultiplier = GetMassMultiplier(InMassClass);
	const double CollisionTypeMultiplier = GetImpactTypeMultiplier(ImpactType);

	const FVectorStabilityLedger::FHitResult Hit = Ledger.ReceiveImpactHit(
		BaseStaggerDamage,
		MassMultiplier,
		CollisionTypeMultiplier);

	if (Hit.bTriggeredStagger)
	{
		OnStaggered.Broadcast();
	}
	return Hit.bAccepted ? Hit.AppliedStabilityDamage : 0.0;
}

void UVectorStabilityComponent::ResetStability()
{
	ApplyConfiguration();
	Ledger.Reset();
}

void UVectorStabilityComponent::ApplyConfiguration()
{
	if (bConfigurationApplied)
	{
		return;
	}

	Ledger.MaximumStability = FMath::Max(1.0, MaximumStability);
	Ledger.MinimumBaseStaggerDamage = FMath::Max(0.0, MinimumBaseStaggerDamage);
	Ledger.MaximumBaseStaggerDamage = FMath::Max(0.0, MaximumBaseStaggerDamage);
	Ledger.WeakpointStaggerMultiplier = FMath::Max(0.0, WeakpointStaggerMultiplier);
	Ledger.UnbalancedDurationSeconds = FMath::Max(0.0, UnbalancedDurationSeconds);
	Ledger.DownedDurationSeconds = FMath::Max(0.0, DownedDurationSeconds);
	Ledger.RisingDurationSeconds = FMath::Max(0.0, RisingDurationSeconds);

	// 若配置了新的稳定度上限，保持当前稳定度不超上限。
	Ledger.Stability = FMath::Min(Ledger.Stability, Ledger.MaximumStability);

	bConfigurationApplied = true;
}

double UVectorStabilityComponent::GetMassMultiplier(const EVectorMassClass InMassClass) const
{
	switch (InMassClass)
	{
	case EVectorMassClass::Light:
		return LightMassMultiplier;
	case EVectorMassClass::Heavy:
		return HeavyMassMultiplier;
	case EVectorMassClass::Medium:
	default:
		return MediumMassMultiplier;
	}
}

double UVectorStabilityComponent::GetImpactTypeMultiplier(const EVectorImpactType ImpactType) const
{
	switch (ImpactType)
	{
	case EVectorImpactType::Wall:
		return WallImpactMultiplier;
	case EVectorImpactType::Ground:
		return GroundImpactMultiplier;
	case EVectorImpactType::Body:
	default:
		return BodyImpactMultiplier;
	}
}
