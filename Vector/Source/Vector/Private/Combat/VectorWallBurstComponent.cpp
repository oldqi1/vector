// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorWallBurstComponent.h"

#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "Combat/VectorKillAttributionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorWallBurst, Log, All);

namespace
{
	UVectorKillAttributionComponent* FindAttribution(const UWorld* World)
	{
		const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
		return GameMode ? GameMode->FindComponentByClass<UVectorKillAttributionComponent>() : nullptr;
	}
}

UVectorWallBurstComponent::UVectorWallBurstComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorWallBurstComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UVectorImpactCollisionComponent* Impact =
		GetOwner() ? GetOwner()->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
	{
		Impact->OnWallImpact.AddUObject(this, &UVectorWallBurstComponent::HandleWallImpact);
	}
}

void UVectorWallBurstComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVectorImpactCollisionComponent* Impact =
		GetOwner() ? GetOwner()->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
	{
		Impact->OnWallImpact.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UVectorWallBurstComponent::HandleWallImpact(
	const double ImpactSpeedCmPerSecond,
	const double SelfDamage)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || ImpactSpeedCmPerSecond <= MinimumTriggerSpeedCmPerSecond)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - LastBurstWorldSeconds < CooldownSeconds)
	{
		return;
	}
	LastBurstWorldSeconds = Now;
	DrawDebugSphere(World, Owner->GetActorLocation(), RadiusCm, 24, FColor::Orange,
		false, 0.45f, 0, 6.0f);

	const UVectorStabilityComponent* SourceStability =
		Owner->FindComponentByClass<UVectorStabilityComponent>();
	const double SourceMassMultiplier = SourceStability
		? SourceStability->GetMassMultiplierByClass(SourceStability->GetMassClass())
		: 1.0;
	const double Damage = FVectorImpactMath::ComputeCollisionDamage(
		ImpactSpeedCmPerSecond,
		SourceMassMultiplier,
		1.0,
		MinimumTriggerSpeedCmPerSecond,
		DamagePerSpeed,
		MaximumDamage);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorWallBurst), false, Owner);
	World->OverlapMultiByObjectType(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
		FCollisionShape::MakeSphere(RadiusCm),
		Params);

	int32 AffectedTargets = 0;
	TSet<TWeakObjectPtr<AActor>> ProcessedTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		const TWeakObjectPtr<AActor> TargetKey(Target);
		if (!Target || Target == Owner || ProcessedTargets.Contains(TargetKey))
		{
			continue;
		}
		ProcessedTargets.Add(TargetKey);
		UVectorStabilityComponent* Stability = Target->FindComponentByClass<UVectorStabilityComponent>();
		UVectorHealthComponent* Health = Target->FindComponentByClass<UVectorHealthComponent>();
		UVectorCharacterMovementComponent* Movement =
			Target->FindComponentByClass<UVectorCharacterMovementComponent>();
		if (Health && Health->IsDead())
		{
			continue;
		}
		if (!Stability && !Health && !Movement)
		{
			continue;
		}

		const FVector Direction = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		const double TargetMass = Stability ? Stability->GetEffectivePhysicalMass() : 2.5;
		if (Movement && !Direction.IsNearlyZero())
		{
			const double Speed = FVectorImpactMath::ComputeMassAdjustedSpeed(
				ImpulseBaseSpeedCmPerSecond, TargetMass);
			Movement->QueueDirectionalVelocityOverride(Direction, Speed);
		}
		if (Stability)
		{
			Stability->ReceiveImpactHit(Damage, Stability->GetMassClass(), EVectorImpactType::Wall);
		}
		const bool bKilled = Health && Health->ApplyDamage(Damage);
		if (bKilled)
		{
			if (UVectorKillAttributionComponent* Attribution = FindAttribution(World))
			{
				Attribution->RecordKill(EVectorKillCause::WallCollision);
			}
		}
		++AffectedTargets;
	}

	UE_LOG(LogVectorWallBurst, Log,
		TEXT("Wall burst: source=%s speed=%.0f selfDamage=%.1f radius=%.0f damage=%.1f targets=%d"),
		*Owner->GetName(), ImpactSpeedCmPerSecond, SelfDamage, RadiusCm, Damage, AffectedTargets);
}
