// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boss/VectorPhysicsBoss.h"

#include "Combat/VectorEnemyAttackComponent.h"
#include "Combat/VectorHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorBoss, Log, All);

AVectorPhysicsBoss::AVectorPhysicsBoss(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Archetype = EVectorEnemyArchetype::HeavyRhinoBeetle;
	MoveSpeedVarianceRatio = 0.0;
	MoveSpeedCmPerSecond = 150.0;
	GetCapsuleComponent()->InitCapsuleSize(105.0f, 120.0f);

	if (HealthComponent)
	{
		// 20 direct full hammer hits, or roughly six capped physical collisions:
		// the intended physics route is about three times faster without making
		// direct damage a soft lock.
		HealthComponent->MaxHealth = 300.0;
	}
	if (StabilityComponent)
	{
		StabilityComponent->MassClass = EVectorMassClass::Heavy;
		StabilityComponent->MaximumStability = 90.0;
		StabilityComponent->PhysicalMassHeavy = 8.0;
		StabilityComponent->StaggeredPhysicalMassHeavy = 4.0;
		StabilityComponent->DownedDurationSeconds = 2.1;
	}
}

void AVectorPhysicsBoss::BeginPlay()
{
	Super::BeginPlay();
	MoveSpeedCmPerSecond = 150.0;
	GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(MoveSpeedCmPerSecond);
	if (AttackComponent)
	{
		AttackComponent->SetComponentTickEnabled(false);
	}
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(
			this, &AVectorPhysicsBoss::HandleBossHealthChanged);
	}
	if (StabilityComponent)
	{
		StabilityComponent->OnStaggered.AddDynamic(
			this, &AVectorPhysicsBoss::HandleBossStaggered);
	}
	ApplyPhaseOutputs(BossState.GetPhase());
	UE_LOG(LogVectorBoss, Log, TEXT("Boss started: actor=%s %s"),
		*GetName(), *BossState.Describe());
}

void AVectorPhysicsBoss::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HealthComponent || HealthComponent->IsDead())
	{
		return;
	}
	AdvanceRam(FMath::Max(0.0, static_cast<double>(DeltaSeconds)));
}

bool AVectorPhysicsBoss::ShouldPauseAI() const
{
	return Super::ShouldPauseAI() || RamPhase != ERamPhase::Waiting;
}

bool AVectorPhysicsBoss::IsExecutingRam() const
{
	return RamPhase == ERamPhase::Active;
}

void AVectorPhysicsBoss::HandleBossHealthChanged(
	const double CurrentHealth,
	const double MaximumHealth,
	const double HealthDelta)
{
	if (HealthDelta >= 0.0 || MaximumHealth <= 0.0)
	{
		return;
	}
	const EVectorPhysicsBossPhase PreviousPhase = BossState.GetPhase();
	if (BossState.ApplyHealthRatio(CurrentHealth / MaximumHealth))
	{
		ApplyPhaseOutputs(PreviousPhase);
	}
}

void AVectorPhysicsBoss::HandleBossStaggered()
{
	const EVectorPhysicsBossPhase PreviousPhase = BossState.GetPhase();
	if (BossState.NotifyStaggered())
	{
		ApplyPhaseOutputs(PreviousPhase);
	}
}

void AVectorPhysicsBoss::BeginRamTelegraph()
{
	APawn* PlayerPawn = FindPlayerPawn();
	if (!PlayerPawn)
	{
		RamPhaseSecondsRemaining = 0.25;
		return;
	}
	LockedRamDirection = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (LockedRamDirection.IsNearlyZero())
	{
		LockedRamDirection = GetActorForwardVector().GetSafeNormal2D();
	}
	RamPhase = ERamPhase::Telegraph;
	RamPhaseSecondsRemaining = FMath::Max(0.01, RamTelegraphSeconds);
	SetAttackWarningPresentation(true);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss ram telegraph: actor=%s phase=%s direction=%s duration=%.2fs mass=%.1f"),
		*GetName(), *BossState.Describe(), *LockedRamDirection.ToCompactString(),
		RamPhaseSecondsRemaining, BossState.GetEffectivePhysicalMass());
}

void AVectorPhysicsBoss::LaunchRam()
{
	SetAttackWarningPresentation(false);
	if (UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		const bool bQueued = Movement->QueueDirectionalVelocityOverride(
			LockedRamDirection, RamSpeedCmPerSecond);
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss ram launch: actor=%s queued=%s speed=%.0f direction=%s physicalMass=%.1f"),
			*GetName(), bQueued ? TEXT("OK") : TEXT("REJECTED"),
			RamSpeedCmPerSecond, *LockedRamDirection.ToCompactString(),
			BossState.GetEffectivePhysicalMass());
	}
	RamPhase = ERamPhase::Active;
	RamPhaseSecondsRemaining = FMath::Max(0.01, RamActiveSeconds);
}

void AVectorPhysicsBoss::AdvanceRam(const double DeltaSeconds)
{
	if (RamPhase == ERamPhase::SlamTelegraph && GetWorld())
	{
		DrawDebugCircle(
			GetWorld(), GetActorLocation() + FVector(0.0, 0.0, 8.0), 400.0,
			48, FColor::Red, false, 0.05f, 0, 10.0f,
			FVector::ForwardVector, FVector::RightVector, false);
	}
	if (RamPhase == ERamPhase::SlamAirborne)
	{
		const UCharacterMovementComponent* CharacterMovement = GetCharacterMovement();
		const bool bHasLeftGround = RamPhaseSecondsRemaining
			< FMath::Max(0.1, SlamMaximumAirborneSeconds) - 0.1;
		if (bHasLeftGround && CharacterMovement && CharacterMovement->IsMovingOnGround())
		{
			RamPhase = ERamPhase::Recovery;
			RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
			UE_LOG(LogVectorBoss, Log,
				TEXT("Boss slam landed: actor=%s recovery=%.2fs (landing shock uses unified impact component)"),
				*GetName(), RamPhaseSecondsRemaining);
			return;
		}
	}

	if (StabilityComponent && StabilityComponent->IsStaggered())
	{
		if (RamPhase == ERamPhase::Telegraph || RamPhase == ERamPhase::SlamTelegraph)
		{
			SetAttackWarningPresentation(false);
			RamPhase = ERamPhase::Recovery;
			RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
			UE_LOG(LogVectorBoss, Log, TEXT("Boss attack telegraph interrupted by stagger: actor=%s"), *GetName());
		}
		return;
	}

	RamPhaseSecondsRemaining -= DeltaSeconds;
	if (RamPhaseSecondsRemaining > 0.0)
	{
		return;
	}

	switch (RamPhase)
	{
	case ERamPhase::Waiting:
		if (APawn* PlayerPawn = FindPlayerPawn())
		{
			if (FVector::Dist2D(GetActorLocation(), PlayerPawn->GetActorLocation()) <= RamTriggerRangeCm)
			{
				if (BossState.GetPhase() != EVectorPhysicsBossPhase::AnchoredShell
					&& bUseSlamForNextAttack)
				{
					BeginSlamTelegraph();
					bUseSlamForNextAttack = false;
				}
				else
				{
					BeginRamTelegraph();
					bUseSlamForNextAttack =
						BossState.GetPhase() != EVectorPhysicsBossPhase::AnchoredShell;
				}
				return;
			}
		}
		RamPhaseSecondsRemaining = 0.25;
		break;
	case ERamPhase::Telegraph:
		LaunchRam();
		break;
	case ERamPhase::SlamTelegraph:
		LaunchSlam();
		break;
	case ERamPhase::Active:
		RamPhase = ERamPhase::Recovery;
		RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
		UE_LOG(LogVectorBoss, Log, TEXT("Boss ram recovery: actor=%s duration=%.2fs"),
			*GetName(), RamPhaseSecondsRemaining);
		break;
	case ERamPhase::SlamAirborne:
		RamPhase = ERamPhase::Recovery;
		RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
		UE_LOG(LogVectorBoss, Warning,
			TEXT("Boss slam airborne timeout: actor=%s recovery=%.2fs"),
			*GetName(), RamPhaseSecondsRemaining);
		break;
	case ERamPhase::Recovery:
		RamPhase = ERamPhase::Waiting;
		RamPhaseSecondsRemaining = BossState.GetRamIntervalSeconds();
		UE_LOG(LogVectorBoss, Verbose, TEXT("Boss ram cycle ready: actor=%s next=%.2fs"),
			*GetName(), RamPhaseSecondsRemaining);
		break;
	default:
		break;
	}
}

void AVectorPhysicsBoss::BeginSlamTelegraph()
{
	RamPhase = ERamPhase::SlamTelegraph;
	RamPhaseSecondsRemaining = FMath::Max(0.01, SlamTelegraphSeconds);
	SetAttackWarningPresentation(true);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss slam telegraph: actor=%s duration=%.2fs radius=400 mass=%.1f"),
		*GetName(), RamPhaseSecondsRemaining, BossState.GetEffectivePhysicalMass());
}

void AVectorPhysicsBoss::LaunchSlam()
{
	SetAttackWarningPresentation(false);
	bool bQueued = false;
	if (UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		bQueued = Movement->QueueDirectionalVelocityOverride(
			FVector::UpVector, SlamLaunchSpeedCmPerSecond);
		if (bQueued)
		{
			Movement->SetMovementMode(MOVE_Falling);
		}
	}
	RamPhase = bQueued ? ERamPhase::SlamAirborne : ERamPhase::Recovery;
	RamPhaseSecondsRemaining = bQueued
		? FMath::Max(0.1, SlamMaximumAirborneSeconds)
		: BossState.GetRecoverySeconds();
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss slam launch: actor=%s queued=%s verticalSpeed=%.0f mode=%s"),
		*GetName(), bQueued ? TEXT("OK") : TEXT("REJECTED"),
		SlamLaunchSpeedCmPerSecond, bQueued ? TEXT("Falling") : TEXT("Recovery"));
}

void AVectorPhysicsBoss::ApplyPhaseOutputs(const EVectorPhysicsBossPhase PreviousPhase)
{
	if (StabilityComponent)
	{
		const double PhaseMass = BossState.GetEffectivePhysicalMass();
		StabilityComponent->PhysicalMassHeavy = PhaseMass;
		StabilityComponent->StaggeredPhysicalMassHeavy = PhaseMass;
	}
	UpdateBossPresentation();
	const EVectorPhysicsBossPhase CurrentPhase = BossState.GetPhase();
	if (CurrentPhase != PreviousPhase)
	{
		OnBossPhaseChanged.Broadcast(PreviousPhase, CurrentPhase);
	}
	UE_LOG(LogVectorBoss, Log, TEXT("Boss phase applied: actor=%s previous=%d current=%d %s"),
		*GetName(), static_cast<int32>(PreviousPhase), static_cast<int32>(CurrentPhase),
		*BossState.Describe());
}

void AVectorPhysicsBoss::UpdateBossPresentation()
{
	FLinearColor Color(0.45f, 0.12f, 0.65f);
	FVector Scale(2.1f, 2.1f, 2.4f);
	switch (BossState.GetPhase())
	{
	case EVectorPhysicsBossPhase::ExposedShell:
		Color = FLinearColor(1.0f, 0.45f, 0.06f);
		Scale = FVector(1.95f, 1.95f, 2.2f);
		break;
	case EVectorPhysicsBossPhase::Overload:
		Color = FLinearColor(1.0f, 0.05f, 0.03f);
		Scale = FVector(1.85f, 1.85f, 2.1f);
		break;
	case EVectorPhysicsBossPhase::Defeated:
		Color = FLinearColor(0.1f, 0.1f, 0.1f);
		break;
	case EVectorPhysicsBossPhase::AnchoredShell:
	default:
		break;
	}
	BaseBodyColor = Color;
	BaseBodyScale = Scale;
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(Scale);
		BodyMesh->SetRelativeLocation(FVector(0.0, 0.0,
			-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 50.0 * Scale.Z));
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

APawn* AVectorPhysicsBoss::FindPlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (APlayerController* PlayerController = Iterator->Get())
		{
			if (PlayerController->GetPawn())
			{
				return PlayerController->GetPawn();
			}
		}
	}
	return nullptr;
}
