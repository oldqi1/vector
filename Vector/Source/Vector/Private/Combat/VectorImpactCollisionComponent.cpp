// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorImpactCollisionComponent.h"

#include "AIController.h"
#include "Combat/VectorEnemy.h"
#include "Combat/VectorEnemyController.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorKillAttributionComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorImpact, Log, All);

namespace
{
	/** 世界级击杀归因账本（GameMode 持有；无则返回 nullptr，不影响玩法）。 */
	UVectorKillAttributionComponent* FindKillAttribution(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		const AGameModeBase* GameMode = World->GetAuthGameMode();
		return GameMode ? GameMode->FindComponentByClass<UVectorKillAttributionComponent>() : nullptr;
	}

	/** 撞击者（GetOwner）是否正在冲锋（角槌兽冲锋撞死他人 → 归因 ChargerRam）。 */
	bool IsChargerRamming(const AActor* Striker)
	{
		const AVectorEnemy* Enemy = Cast<AVectorEnemy>(Striker);
		if (!Enemy)
		{
			return false;
		}
		const AController* Controller = Enemy->GetController();
		const AVectorEnemyController* EnemyController = Controller
			? Cast<AVectorEnemyController>(Controller)
			: nullptr;
		return EnemyController && EnemyController->IsCharging();
	}

	FVector GetEffectiveVelocity(const AActor* Actor)
	{
		if (!Actor)
		{
			return FVector::ZeroVector;
		}
		if (const UVectorCharacterMovementComponent* Movement =
			Actor->FindComponentByClass<UVectorCharacterMovementComponent>())
		{
			return Movement->GetEffectiveVelocityForPendingStep();
		}
		return Actor->GetVelocity();
	}

	FVector ComputeCollisionDirection(
		const AActor* Owner,
		const AActor* Target,
		const FHitResult& Hit,
		const FVector& MoveDelta)
	{
		if (!Target)
		{
			// A horizontal hard-surface normal is a wall. A vertical normal is floor
			// contact and must remain zero here; landing has its own vertical path.
			return FVector::VectorPlaneProject(
				-Hit.ImpactNormal, FVector::UpVector).GetSafeNormal();
		}
		FVector Direction = Owner
			? (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D()
			: FVector::ZeroVector;
		if (Direction.IsNearlyZero())
		{
			Direction = MoveDelta.GetSafeNormal2D();
		}
		return Direction;
	}
}

UVectorImpactCollisionComponent::UVectorImpactCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorImpactCollisionComponent::OnCharacterImpact(
	const FHitResult& Hit,
	const FVector& MoveDelta)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	const bool bHitDamageableTarget = HitActor
		&& HitActor != Owner
		&& (HitActor->FindComponentByClass<UVectorStabilityComponent>() != nullptr
			|| HitActor->FindComponentByClass<UVectorHealthComponent>() != nullptr);
	const FVector CollisionDirection = ComputeCollisionDirection(
		Owner,
		bHitDamageableTarget ? HitActor : nullptr,
		Hit,
		MoveDelta);
	const FVector StrikerVelocity = GetEffectiveVelocity(Owner);
	const FVector TargetVelocity = bHitDamageableTarget
		? GetEffectiveVelocity(HitActor)
		: FVector::ZeroVector;
	const double ClosingSpeed = FVectorImpactMath::ComputePlanarClosingSpeed(
		StrikerVelocity,
		TargetVelocity,
		CollisionDirection);
	if (bHitDamageableTarget)
	{
		// 先通知约束停止后续收绳；本函数随后写入的反弹速度保持 last-write-wins。
		OnBodyImpact.Broadcast(HitActor);
	}
	else if (!CollisionDirection.IsNearlyZero())
	{
		OnSurfaceContact.Broadcast(ClosingSpeed);
	}

	if (ClosingSpeed < MinDamageSpeedCmPerSecond)
	{
		UE_LOG(LogVectorImpact, Verbose, TEXT("Impact ignored: owner=%s hit=%s closing=%.0f min=%.0f direction=%s"),
			*Owner->GetName(),
			HitActor ? *HitActor->GetName() : TEXT("(world/terrain)"),
			ClosingSpeed,
			MinDamageSpeedCmPerSecond,
			*CollisionDirection.ToCompactString());
		return;
	}

	UE_LOG(LogVectorImpact, Log, TEXT("Impact resolved: owner=%s hit=%s closing=%.0f strikerV=%s targetV=%s direction=%s damageable=%d"),
		*Owner->GetName(),
		HitActor ? *HitActor->GetName() : TEXT("(world/terrain)"),
		ClosingSpeed,
		*StrikerVelocity.ToCompactString(),
		*TargetVelocity.ToCompactString(),
		*CollisionDirection.ToCompactString(),
		bHitDamageableTarget ? 1 : 0);

	if (bHitDamageableTarget)
	{
		ResolveTargetCollision(HitActor, CollisionDirection, StrikerVelocity, TargetVelocity, ClosingSpeed);
	}
	else
	{
		// 撞到硬表面（墙/地面/障碍）：撞击者自反噬。
		ResolveSurfaceCollision(ClosingSpeed);
	}
}

void UVectorImpactCollisionComponent::OnLandedWithImpact(const double FallSpeedCmPerSecond)
{
	if (!bEnableLandingShock || FallSpeedCmPerSecond > -MinFallSpeedCmPerSecond)
	{
		return;
	}
	UE_LOG(LogVectorImpact, Log, TEXT("Landing shock triggered: owner=%s fall=%.0f threshold=%.0f radius=%.0f"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
		-FallSpeedCmPerSecond,
		MinFallSpeedCmPerSecond,
		LandedAoERadiusCm);
	ResolveLandingShock(-FallSpeedCmPerSecond);
}

void UVectorImpactCollisionComponent::ResolveTargetCollision(
	AActor* TargetActor,
	const FVector& CollisionDirection,
	const FVector& StrikerVelocity,
	const FVector& TargetVelocity,
	const double ClosingSpeedCmPerSecond)
{
	AActor* Owner = GetOwner();
	if (!Owner || !TargetActor || CollisionDirection.IsNearlyZero())
	{
		return;
	}
	const FString OwnerName = Owner->GetName();
	const FString TargetName = TargetActor->GetName();
	UVectorHealthComponent* StrikerHealth = Owner->FindComponentByClass<UVectorHealthComponent>();
	UVectorHealthComponent* TargetHealth = TargetActor->FindComponentByClass<UVectorHealthComponent>();
	UVectorStabilityComponent* StrikerStability = Owner->FindComponentByClass<UVectorStabilityComponent>();
	UVectorStabilityComponent* TargetStability = TargetActor->FindComponentByClass<UVectorStabilityComponent>();
	if (TargetHealth && TargetHealth->IsDead())
	{
		return;
	}
	if (!TargetHealth && !TargetStability)
	{
		return;
	}

	const double StrikerDamageMassMultiplier = StrikerStability
		? StrikerStability->GetMassMultiplierByClass(StrikerStability->GetMassClass())
		: 1.0;
	const double DamageToTarget = FVectorImpactMath::ComputeCollisionDamage(
		ClosingSpeedCmPerSecond,
		StrikerDamageMassMultiplier,
		BodyCollisionMultiplier,
		MinDamageSpeedCmPerSecond,
		DamagePerSpeed,
		MaxDamage);
	const double TargetDamageMassMultiplier = TargetStability
		? TargetStability->GetMassMultiplierByClass(TargetStability->GetMassClass())
		: 1.0;
	// Monster bodies hurt each other in the same physical event. The player has
	// no Stability component, so enemy-player contacts retain directional damage.
	const double DamageToStriker = StrikerStability && TargetStability
		&& (!StrikerHealth || !StrikerHealth->IsDead())
		? FVectorImpactMath::ComputeCollisionDamage(
			ClosingSpeedCmPerSecond,
			TargetDamageMassMultiplier,
			BodyCollisionMultiplier,
			MinDamageSpeedCmPerSecond,
			DamagePerSpeed,
			MaxDamage)
		: 0.0;
	// Keep the established target-side name/log contract while adding the
	// reciprocal monster-side consequence below.
	const double Damage = DamageToTarget;
	const bool bTargetWasRamming = IsChargerRamming(TargetActor);
	const FString StrikerMassClassName = StrikerStability
		? UEnum::GetValueAsString(StrikerStability->GetMassClass()) : TEXT("N/A");
	const FString TargetMassClassName = TargetStability
		? UEnum::GetValueAsString(TargetStability->GetMassClass()) : TEXT("N/A");

	const double StrikerMass = StrikerStability
		? StrikerStability->GetEffectivePhysicalMass()
		: DefaultPhysicalMass;
	const double TargetMass = TargetStability
		? TargetStability->GetEffectivePhysicalMass()
		: DefaultPhysicalMass;
	const double StrikerSpeedBefore = FVector::DotProduct(StrikerVelocity, CollisionDirection);
	const double TargetSpeedBefore = FVector::DotProduct(TargetVelocity, CollisionDirection);
	double StrikerSpeedAfter = 0.0;
	double TargetSpeedAfter = 0.0;
	if (!FVectorImpactMath::SolveOneDimensionalCollision(
		StrikerSpeedBefore,
		TargetSpeedBefore,
		StrikerMass,
		TargetMass,
		CollisionRestitution,
		StrikerSpeedAfter,
		TargetSpeedAfter))
	{
		return;
	}

	const FVector StrikerVelocityAfter = StrikerVelocity
		+ CollisionDirection * (StrikerSpeedAfter - StrikerSpeedBefore);
	const FVector TargetVelocityAfter = TargetVelocity
		+ CollisionDirection * (TargetSpeedAfter - TargetSpeedBefore);
	if (UVectorCharacterMovementComponent* OwnerMovement =
		Owner->FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		OwnerMovement->QueueWorldVelocityOverride(StrikerVelocityAfter);
	}
	if (UVectorCharacterMovementComponent* TargetMovement =
		TargetActor->FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		TargetMovement->QueueWorldVelocityOverride(TargetVelocityAfter);
	}

	const double MomentumBefore = StrikerMass * StrikerSpeedBefore + TargetMass * TargetSpeedBefore;
	const double MomentumAfter = StrikerMass * StrikerSpeedAfter + TargetMass * TargetSpeedAfter;
	const double EnergyBefore = 0.5 * StrikerMass * FMath::Square(StrikerSpeedBefore)
		+ 0.5 * TargetMass * FMath::Square(TargetSpeedBefore);
	const double EnergyAfter = 0.5 * StrikerMass * FMath::Square(StrikerSpeedAfter)
		+ 0.5 * TargetMass * FMath::Square(TargetSpeedAfter);
	const bool bMomentumConserved = FMath::IsNearlyEqual(
		MomentumBefore,
		MomentumAfter,
		FMath::Max(0.01, FMath::Abs(MomentumBefore) * 1.e-6));
	const bool bEnergyDidNotIncrease = EnergyAfter <= EnergyBefore
		+ FMath::Max(0.01, EnergyBefore * 1.e-6);
	UE_LOG(LogVectorImpact, Log,
		TEXT("Collision solve: %s(m=%.2f,u=%.0f)->%s(m=%.2f,u=%.0f) closing=%.0f e=%.2f result=(%.0f,%.0f) momentum=%.1f->%.1f energy=%.0f->%.0f check=%s"),
		*OwnerName, StrikerMass, StrikerSpeedBefore,
		*TargetName, TargetMass, TargetSpeedBefore,
		ClosingSpeedCmPerSecond, FMath::Clamp(CollisionRestitution, 0.0, 1.0),
		StrikerSpeedAfter, TargetSpeedAfter,
		MomentumBefore, MomentumAfter, EnergyBefore, EnergyAfter,
		bMomentumConserved && bEnergyDidNotIncrease ? TEXT("PASS") : TEXT("FAIL"));
	if (!bMomentumConserved || !bEnergyDidNotIncrease)
	{
		UE_LOG(LogVectorImpact, Error,
			TEXT("Collision invariant failed: momentumConserved=%d energyDidNotIncrease=%d"),
			bMomentumConserved ? 1 : 0,
			bEnergyDidNotIncrease ? 1 : 0);
	}

	// 撞飞即失衡：同步扣稳定度（连锁中目标越来越容易失衡倒地）。
	if (TargetStability)
	{
		TargetStability->ReceiveImpactHit(Damage, TargetStability->GetMassClass(), EVectorImpactType::Body);
	}
	// 核心生命（击杀层）；致死瞬间上报击杀归因（借冲锋 vs 普通撞怪）。
	const bool bKilled = TargetHealth && TargetHealth->ApplyDamage(Damage);
	if (bKilled)
	{
		if (UVectorKillAttributionComponent* Attribution = FindKillAttribution(GetWorld()))
		{
			const EVectorKillCause Cause = IsChargerRamming(GetOwner())
				? EVectorKillCause::ChargerRam
				: EVectorKillCause::BodyCollision;
			Attribution->RecordKill(Cause);
		}
	}

	bool bStrikerKilled = false;
	if (DamageToStriker > 0.0)
	{
		StrikerStability->ReceiveImpactHit(
			DamageToStriker,
			StrikerStability->GetMassClass(),
			EVectorImpactType::Body);
		bStrikerKilled = StrikerHealth && StrikerHealth->ApplyDamage(DamageToStriker);
		if (bStrikerKilled)
		{
			if (UVectorKillAttributionComponent* Attribution = FindKillAttribution(GetWorld()))
			{
				const EVectorKillCause Cause = bTargetWasRamming
					? EVectorKillCause::ChargerRam
					: EVectorKillCause::BodyCollision;
				Attribution->RecordKill(Cause);
			}
		}
	}

	UE_LOG(LogVectorImpact, Log, TEXT("Body collision: %s -> %s closing=%.0f damage(target=%.1f,striker=%.1f) massClass=(%s,%s) killed=(target=%s,striker=%s)"),
		*OwnerName,
		*TargetName,
		ClosingSpeedCmPerSecond,
		DamageToTarget,
		DamageToStriker,
		*StrikerMassClassName,
		*TargetMassClassName,
		bKilled ? TEXT("YES") : TEXT("no"),
		bStrikerKilled ? TEXT("YES") : TEXT("no"));
}

void UVectorImpactCollisionComponent::ResolveSurfaceCollision(const double ImpactSpeedCmPerSecond)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	UVectorStabilityComponent* SelfStability = Owner->FindComponentByClass<UVectorStabilityComponent>();
	UVectorHealthComponent* SelfHealth = Owner->FindComponentByClass<UVectorHealthComponent>();

	const double MassMultiplier = SelfStability
		? SelfStability->GetMassMultiplierByClass(SelfStability->GetMassClass())
		: 1.0;
	const double Damage = FVectorImpactMath::ComputeCollisionDamage(
		ImpactSpeedCmPerSecond,
		MassMultiplier,
		WallCollisionMultiplier,
		MinDamageSpeedCmPerSecond,
		DamagePerSpeed,
		MaxDamage);

	// 撞墙：自反噬（生命 + 稳定度同步）。
	if (SelfStability)
	{
		SelfStability->ReceiveImpactHit(Damage, SelfStability->GetMassClass(), EVectorImpactType::Wall);
	}
	OnWallImpact.Broadcast(ImpactSpeedCmPerSecond, Damage);
	const bool bKilled = SelfHealth && SelfHealth->ApplyDamage(Damage);
	if (bKilled)
	{
		if (UVectorKillAttributionComponent* Attribution = FindKillAttribution(GetWorld()))
		{
			Attribution->RecordKill(EVectorKillCause::WallCollision);
		}
	}

	UE_LOG(LogVectorImpact, Log, TEXT("Wall collision: %s self-damage=%.1f speed=%.0f killed=%s"),
		*Owner->GetName(),
		Damage,
		ImpactSpeedCmPerSecond,
		bKilled ? TEXT("YES") : TEXT("no"));
}

void UVectorImpactCollisionComponent::ResolveLandingShock(const double FallSpeedCmPerSecond)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	const UVectorStabilityComponent* OwnerStability =
		Owner->FindComponentByClass<UVectorStabilityComponent>();
	const double SourceMassMultiplier = OwnerStability
		? OwnerStability->GetMassMultiplierByClass(OwnerStability->GetMassClass())
		: 1.0;

	FCollisionShape Shape = FCollisionShape::MakeSphere(LandedAoERadiusCm);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VectorLandingShock), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
		Shape,
		QueryParams);

	TSet<TWeakObjectPtr<AActor>> ProcessedTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		const TWeakObjectPtr<AActor> TargetKey(HitActor);
		if (!HitActor || HitActor == Owner || ProcessedTargets.Contains(TargetKey))
		{
			continue;
		}
		ProcessedTargets.Add(TargetKey);
		UVectorStabilityComponent* TargetStability = HitActor->FindComponentByClass<UVectorStabilityComponent>();
		UVectorHealthComponent* TargetHealth = HitActor->FindComponentByClass<UVectorHealthComponent>();
		if (TargetHealth && TargetHealth->IsDead())
		{
			continue;
		}
		if (!TargetStability && !TargetHealth)
		{
			continue;
		}
		const double Damage = FVectorImpactMath::ComputeCollisionDamage(
			FallSpeedCmPerSecond,
			SourceMassMultiplier,
			WallCollisionMultiplier, // 落地震荡按地面（硬表面）强度
			MinDamageSpeedCmPerSecond,
			DamagePerSpeed,
			MaxDamage);
		if (Damage > 0.0)
		{
			if (TargetStability)
			{
				TargetStability->ReceiveImpactHit(Damage, TargetStability->GetMassClass(), EVectorImpactType::Ground);
			}
			const bool bKilled = TargetHealth && TargetHealth->ApplyDamage(Damage);
			if (bKilled)
			{
				if (UVectorKillAttributionComponent* Attribution = FindKillAttribution(GetWorld()))
				{
					Attribution->RecordKill(EVectorKillCause::LandingShock);
				}
			}
			UE_LOG(LogVectorImpact, Log, TEXT("Landing shock: %s -> %s fall=%.0f damage=%.1f killed=%s"),
				*Owner->GetName(),
				*HitActor->GetName(),
				FallSpeedCmPerSecond,
				Damage,
				bKilled ? TEXT("YES") : TEXT("no"));
		}
	}
}
