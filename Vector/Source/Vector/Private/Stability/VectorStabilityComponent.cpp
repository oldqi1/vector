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
		// 数值已同步过；但失衡时长按质量档动态生效（敌人 BeginPlay 才设置 MassClass），
		// 因此时长部分每次同步，不提前锁定。
		SyncStaggerDurations();
		return;
	}

	Ledger.MaximumStability = FMath::Max(1.0, MaximumStability);
	Ledger.MinimumBaseStaggerDamage = FMath::Max(0.0, MinimumBaseStaggerDamage);
	Ledger.MaximumBaseStaggerDamage = FMath::Max(0.0, MaximumBaseStaggerDamage);
	Ledger.WeakpointStaggerMultiplier = FMath::Max(0.0, WeakpointStaggerMultiplier);
	SyncStaggerDurations();

	// 若配置了新的稳定度上限，保持当前稳定度不超上限。
	Ledger.Stability = FMath::Min(Ledger.Stability, Ledger.MaximumStability);

	bConfigurationApplied = true;
}

void UVectorStabilityComponent::SyncStaggerDurations()
{
	// 轻质量（弹药）失衡时长按倍率缩短：被锤直接飞出去继续连锁，不打断节奏；
	// 中/重质量保持标准硬直（重物失衡 = 脱锚黄金窗口）。
	const double Multiplier = (MassClass == EVectorMassClass::Light)
		? FMath::Clamp(LightStaggerDurationMultiplier, 0.0, 1.0)
		: 1.0;

	Ledger.UnbalancedDurationSeconds = FMath::Max(0.0, UnbalancedDurationSeconds * Multiplier);
	Ledger.DownedDurationSeconds = FMath::Max(0.0, DownedDurationSeconds * Multiplier);
	Ledger.RisingDurationSeconds = FMath::Max(0.0, RisingDurationSeconds * Multiplier);
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

double UVectorStabilityComponent::GetEffectivePhysicalMass() const
{
	const bool bUseStaggeredMass = IsStaggered();
	switch (MassClass)
	{
	case EVectorMassClass::Light:
		return FMath::Max(0.1, bUseStaggeredMass ? StaggeredPhysicalMassLight : PhysicalMassLight);
	case EVectorMassClass::Heavy:
		return FMath::Max(0.1, bUseStaggeredMass ? StaggeredPhysicalMassHeavy : PhysicalMassHeavy);
	case EVectorMassClass::Medium:
	default:
		return FMath::Max(0.1, bUseStaggeredMass ? StaggeredPhysicalMassMedium : PhysicalMassMedium);
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
