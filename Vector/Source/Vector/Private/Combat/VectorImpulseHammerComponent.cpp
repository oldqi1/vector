// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorImpulseHammerComponent.h"

#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "Combat/VectorEnemy.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorKillAttributionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorCombat, Log, All);

namespace
{
	const TCHAR* GetPhaseLabel(const EVectorActionPhase Phase)
	{
		switch (Phase)
		{
		case EVectorActionPhase::Idle: return TEXT("Idle");
		case EVectorActionPhase::Windup: return TEXT("Windup");
		case EVectorActionPhase::Active: return TEXT("Active");
		case EVectorActionPhase::Recovery: return TEXT("Recovery");
		default: return TEXT("Unknown");
		}
	}

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
}

UVectorImpulseHammerComponent::UVectorImpulseHammerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UVectorImpulseHammerComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const EVectorActionPhase PreviousPhase = Timeline.Phase;
	Timeline.Advance(DeltaTime);
	if (PreviousPhase != EVectorActionPhase::Idle && Timeline.Phase == EVectorActionPhase::Idle)
	{
		ReleaseActionLock();
	}

	// 蓄力期可视化：施力方向 + 进度（灰盒可读性，bDrawChargeDebug 可关）。
	if (bDrawChargeDebug && IsCharging())
	{
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		if (World && Owner)
		{
			const FVector Direction = ComputeStrikeDirection();
			const double Reach = HitReachCm * Timeline.ChargeProgress;
			DrawDebugDirectionalArrow(
				World,
				Owner->GetActorLocation(),
				Owner->GetActorLocation() + Direction * Reach,
				12.0f,
				ComputeChargeDebugColor().ToFColor(true),
				false,
				0.02f);
		}
	}
	// Recovery 冷却可视化：头顶灰色圆环，缺口 = 剩余冷却（传统 CD 圆环读法）。
	else if (bDrawChargeDebug && Timeline.Phase == EVectorActionPhase::Recovery)
	{
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		if (World && Owner)
		{
			const double Total = FMath::Max(UE_SMALL_NUMBER, Timeline.RecoverySeconds);
			const double Ratio = FMath::Clamp(Timeline.RecoverySecondsRemaining / Total, 0.0, 1.0);
			// 从正上方（90°）顺时针扫过 Ratio*360°；扫过部分=已冷却，缺口=剩余。
			// DrawDebugArc 在 UE 5.8 不存在，用 DrawDebugLine 分段描弧。
			const FVector Center = Owner->GetActorLocation() + FVector(0.0, 0.0, 200.0);
			constexpr double ArcRadiusCm = 80.0;
			constexpr double StartAngleDeg = 90.0;
			const double EndAngleDeg = StartAngleDeg + 360.0 * Ratio;
			constexpr int32 ArcSegments = 32;
			for (int32 Index = 0; Index < ArcSegments; ++Index)
			{
				const double Angle0Deg = FMath::Lerp(StartAngleDeg, EndAngleDeg, static_cast<double>(Index) / ArcSegments);
				const double Angle1Deg = FMath::Lerp(StartAngleDeg, EndAngleDeg, static_cast<double>(Index + 1) / ArcSegments);
				const double Rad0 = FMath::DegreesToRadians(Angle0Deg);
				const double Rad1 = FMath::DegreesToRadians(Angle1Deg);
				const FVector Point0 = Center + FVector(FMath::Cos(Rad0), FMath::Sin(Rad0), 0.0) * ArcRadiusCm;
				const FVector Point1 = Center + FVector(FMath::Cos(Rad1), FMath::Sin(Rad1), 0.0) * ArcRadiusCm;
				DrawDebugLine(World, Point0, Point1, FColor(150, 150, 150), false, 0.02f, 0, 2.5f);
			}
		}
	}
}

void UVectorImpulseHammerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAction();
	Super::EndPlay(EndPlayReason);
}

void UVectorImpulseHammerComponent::StartCharge()
{
	if (Timeline.IsBusy())
	{
		UE_LOG(LogVectorCombat, Log, TEXT("Hammer StartCharge -> REJECTED (own Phase=%s)"),
			GetPhaseLabel(Timeline.Phase));
		return;
	}
	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		if (!Lock->TryAcquire(this, TEXT("ImpulseHammer")))
		{
			UE_LOG(LogVectorCombat, Log, TEXT("Hammer StartCharge -> REJECTED (action lock=%s)"),
				*Lock->GetActiveActionName().ToString());
			return;
		}
		bOwnsActionLock = true;
	}
	const bool bStarted = Timeline.TryStartWindup();
	if (!bStarted)
	{
		ReleaseActionLock();
	}
	UE_LOG(LogVectorCombat, Log, TEXT("Hammer StartCharge -> %s (Phase=%s)"),
		bStarted ? TEXT("OK") : TEXT("REJECTED"),
		GetPhaseLabel(Timeline.Phase));
}

void UVectorImpulseHammerComponent::CancelAction()
{
	Timeline.Reset();
	ReleaseActionLock();
}

void UVectorImpulseHammerComponent::ReleaseCharge()
{
	const bool bReleased = Timeline.TryRelease();
	UE_LOG(LogVectorCombat, Log, TEXT("Hammer ReleaseCharge -> %s (Phase=%s, Charge=%.2f)"),
		bReleased ? TEXT("OK") : TEXT("REJECTED"),
		GetPhaseLabel(Timeline.Phase),
		Timeline.ChargeProgress);
	if (!bReleased)
	{
		return;
	}
	ApplyImpulseToHitActors(ComputeStrikeDirection());
}

FVector UVectorImpulseHammerComponent::ComputeStrikeDirection() const
{
	return FVectorCombatTargeting::ComputeCursorGroundAimDirection(GetOwner());
}

void UVectorImpulseHammerComponent::ApplyImpulseToHitActors(const FVector& Direction)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	// 沿施力方向做球形扫描：SweepMulti 对 Block 与 Overlap 响应都返回命中
	// （OverlapMulti 只返回 Overlap 响应，会漏掉默认对 Pawn 为 Block 的角色胶囊）。
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Direction * HitReachCm;
	FCollisionShape Shape = FCollisionShape::MakeSphere(HitRadiusCm);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VectorImpulseHammer), false, Owner);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		Shape,
		QueryParams);

	AActor* BestTarget = nullptr;
	float BestDistance = MAX_FLT;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Owner)
		{
			continue;
		}
		if (const UVectorHealthComponent* Health =
			HitActor->FindComponentByClass<UVectorHealthComponent>(); Health && Health->IsDead())
		{
			continue;
		}
		const bool bHasMovement = HitActor->FindComponentByClass<UVectorCharacterMovementComponent>() != nullptr;
		const bool bHasStability = HitActor->FindComponentByClass<UVectorStabilityComponent>() != nullptr;
		if (!bHasMovement && !bHasStability)
		{
			continue;
		}
		// 取沿施力方向最近的可推目标。
		if (Hit.Distance < BestDistance)
		{
			BestDistance = Hit.Distance;
			BestTarget = HitActor;
		}
	}

	if (!BestTarget)
	{
		UE_LOG(LogVectorCombat, Log, TEXT("Hammer release: NO valid target (sweep hits=%d)"), Hits.Num());
		return;
	}

	const double Charge = FMath::Clamp(Timeline.ChargeProgress, 0.05, 1.0);

	// 稳定度伤害：基础值 × 蓄力进度（弱击 5% 起，避免零伤害无感）。
	if (UVectorStabilityComponent* Stability = BestTarget->FindComponentByClass<UVectorStabilityComponent>())
	{
		const double Applied = Stability->ReceiveImpactHit(BaseStaggerDamage * Charge, Stability->GetMassClass(), EVectorImpactType::Body);
		UE_LOG(LogVectorCombat, Log, TEXT("Hammer hit %s (Mass=%s): stability damage=%.1f"),
			*BestTarget->GetName(),
			*UEnum::GetValueAsString(Stability->GetMassClass()),
			Applied);
	}

	// 保底核心生命伤害（物理不能成软锁：不靠碰撞也能磨死）。
	if (UVectorHealthComponent* Health = BestTarget->FindComponentByClass<UVectorHealthComponent>())
	{
		const double HealthDamage = BaseHealthDamage * Charge;
		if (HealthDamage >= Health->GetHealth())
		{
			if (AVectorEnemy* Enemy = Cast<AVectorEnemy>(BestTarget))
			{
				Enemy->PrepareForHammerLethalLaunch();
			}
		}
		const bool bKilled = Health->ApplyDamage(HealthDamage);
		if (bKilled)
		{
			if (UVectorKillAttributionComponent* Attribution = FindKillAttribution(GetWorld()))
			{
				Attribution->RecordKill(EVectorKillCause::Hammer);
			}
		}
		UE_LOG(LogVectorCombat, Log, TEXT("Hammer hit %s: health damage=%.1f killed=%s"),
			*BestTarget->GetName(),
			HealthDamage,
			bKilled ? TEXT("YES") : TEXT("no"));
	}

	// 受控冲量（动量模型）：Δv = 冲量(I = ImpulseSpeed × Charge) ÷ 相对质量。
	// 失衡（脱锚）目标使用失衡质量表——重型平时像山，失衡后成为最凶的炮弹
	// （对齐设计案"失衡状态：可被推出/改变属性"）。
	if (UVectorCharacterMovementComponent* Movement = BestTarget->FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		const UVectorStabilityComponent* Stability = BestTarget->FindComponentByClass<UVectorStabilityComponent>();
		const bool bStaggered = Stability && Stability->IsStaggered();
		const double MassValue = Stability ? Stability->GetEffectivePhysicalMass() : 2.5;
		const double TargetSpeed = ImpulseSpeedCmPerSecond * Charge
			/ FMath::Max(UE_SMALL_NUMBER, MassValue);
		if (TargetSpeed > 1.0)
		{
			const bool bQueued = Movement->QueueDirectionalVelocityOverride(Direction, TargetSpeed);
			UE_LOG(LogVectorCombat, Log, TEXT("Hammer impulse -> %s targetSpeed=%.0f cm/s (I=%.0f, mass=%.2f, staggered=%s) charge=%.2f queued=%s"),
				*BestTarget->GetName(),
				TargetSpeed,
				ImpulseSpeedCmPerSecond * Charge,
				MassValue,
				bStaggered ? TEXT("YES") : TEXT("no"),
				Charge,
				bQueued ? TEXT("OK") : TEXT("REJECTED"));
		}
		else
		{
			UE_LOG(LogVectorCombat, Log, TEXT("Hammer impulse skipped (targetSpeed=%.1f too low)"), TargetSpeed);
		}
	}
}

FLinearColor UVectorImpulseHammerComponent::ComputeChargeDebugColor() const
{
	// 蓄力进度 0~1：绿 → 黄 → 红，力度一眼可读。
	const double Progress = FMath::Clamp(Timeline.ChargeProgress, 0.0, 1.0);
	return FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Progress);
}

void UVectorImpulseHammerComponent::ReleaseActionLock()
{
	if (!bOwnsActionLock)
	{
		return;
	}
	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		Lock->Release(this);
	}
	bOwnsActionLock = false;
}
