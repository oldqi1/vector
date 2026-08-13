// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorImpactCollisionComponent.h"

#include "Combat/VectorHealthComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorImpact, Log, All);

UVectorImpactCollisionComponent::UVectorImpactCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVectorImpactCollisionComponent::OnCharacterImpact(
	const FHitResult& Hit,
	const double ImpactSpeedCmPerSecond,
	const FVector& MoveDelta)
{
	AActor* Owner = GetOwner();
	if (!Owner || ImpactSpeedCmPerSecond < MinDamageSpeedCmPerSecond)
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	const bool bHitStabilityTarget = HitActor
		&& HitActor != Owner
		&& HitActor->FindComponentByClass<UVectorStabilityComponent>() != nullptr;

	if (bHitStabilityTarget)
	{
		ResolveTargetCollision(HitActor, MoveDelta.GetSafeNormal(), ImpactSpeedCmPerSecond);
	}
	else
	{
		// 撞到硬表面（墙/地面/障碍）：撞击者自反噬。
		ResolveSurfaceCollision(ImpactSpeedCmPerSecond);
	}
}

void UVectorImpactCollisionComponent::OnLandedWithImpact(const double FallSpeedCmPerSecond)
{
	if (!bEnableLandingShock || FallSpeedCmPerSecond > -MinFallSpeedCmPerSecond)
	{
		return;
	}
	ResolveLandingShock(-FallSpeedCmPerSecond);
}

void UVectorImpactCollisionComponent::ResolveTargetCollision(
	AActor* TargetActor,
	const FVector& MoveDirection,
	const double ImpactSpeedCmPerSecond)
{
	UVectorHealthComponent* TargetHealth = TargetActor->FindComponentByClass<UVectorHealthComponent>();
	UVectorStabilityComponent* TargetStability = TargetActor->FindComponentByClass<UVectorStabilityComponent>();
	if (!TargetHealth && !TargetStability)
	{
		return;
	}

	// 碰撞伤害（击杀主力）＝ 速度（超阈值部分）× 每速 × 对方质量系数 × 身体互撞系数，硬上限。
	const double MassMultiplier = TargetStability
		? TargetStability->GetMassMultiplierByClass(TargetStability->GetMassClass())
		: 1.0;
	const double Damage = FVectorImpactMath::ComputeCollisionDamage(
		ImpactSpeedCmPerSecond,
		MassMultiplier,
		BodyCollisionMultiplier,
		MinDamageSpeedCmPerSecond,
		DamagePerSpeed,
		MaxDamage);

	// 撞飞即失衡：同步扣稳定度（连锁中目标越来越容易失衡倒地）。
	if (TargetStability)
	{
		TargetStability->ReceiveImpactHit(Damage, TargetStability->GetMassClass(), EVectorImpactType::Body);
	}
	// 核心生命（击杀层）。
	const bool bKilled = TargetHealth && TargetHealth->ApplyDamage(Damage);

	UE_LOG(LogVectorImpact, Log, TEXT("Body collision: %s -> %s speed=%.0f damage=%.1f (Mass=%s, killed=%s)"),
		*GetOwner()->GetName(),
		*TargetActor->GetName(),
		ImpactSpeedCmPerSecond,
		Damage,
		TargetStability ? *UEnum::GetValueAsString(TargetStability->GetMassClass()) : TEXT("N/A"),
		bKilled ? TEXT("YES") : TEXT("no"));

	// 动量传递雏形：被撞目标获得撞击速度的一部分，形成连锁。
	if (UVectorCharacterMovementComponent* TargetMovement = TargetActor->FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		const double TransferredSpeed = ImpactSpeedCmPerSecond * FMath::Clamp(MomentumTransferRatio, 0.0, 1.0);
		if (TransferredSpeed > 1.0)
		{
			TargetMovement->QueueWorldVelocityChange(MoveDirection * TransferredSpeed);
			UE_LOG(LogVectorImpact, Log, TEXT("Momentum transfer: %s gains %.0f cm/s along %s"),
				*TargetActor->GetName(),
				TransferredSpeed,
				*MoveDirection.ToCompactString());
		}
	}
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
	const bool bKilled = SelfHealth && SelfHealth->ApplyDamage(Damage);

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

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == Owner)
		{
			continue;
		}
		UVectorStabilityComponent* TargetStability = HitActor->FindComponentByClass<UVectorStabilityComponent>();
		UVectorHealthComponent* TargetHealth = HitActor->FindComponentByClass<UVectorHealthComponent>();
		if (!TargetStability && !TargetHealth)
		{
			continue;
		}
		const double MassMultiplier = TargetStability
			? TargetStability->GetMassMultiplierByClass(TargetStability->GetMassClass())
			: 1.0;
		const double Damage = FVectorImpactMath::ComputeCollisionDamage(
			FallSpeedCmPerSecond,
			MassMultiplier,
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
			UE_LOG(LogVectorImpact, Log, TEXT("Landing shock: %s -> %s fall=%.0f damage=%.1f killed=%s"),
				*Owner->GetName(),
				*HitActor->GetName(),
				FallSpeedCmPerSecond,
				Damage,
				bKilled ? TEXT("YES") : TEXT("no"));
		}
	}
}
