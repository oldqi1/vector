// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boss/VectorPhysicsBoss.h"

#include "Boss/VectorKineticOrb.h"
#include "Combat/VectorBreakableAnchorComponent.h"
#include "Combat/VectorEnemyAttackComponent.h"
#include "Combat/VectorHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stability/VectorStabilityComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorBoss, Log, All);

namespace
{
	void DrawBallisticArc(
		UWorld* World,
		const FVector& Start,
		const FVector& InitialVelocity,
		const double GravityZ,
		const double FlightSeconds,
		const FColor& Color)
	{
		if (!World || FlightSeconds <= 0.0)
		{
			return;
		}
		constexpr int32 SegmentCount = 20;
		FVector PreviousPoint = Start;
		for (int32 Index = 1; Index <= SegmentCount; ++Index)
		{
			const double Time = FlightSeconds * static_cast<double>(Index) / SegmentCount;
			const FVector Point = FVectorImpactMath::SampleBallisticPosition(
				Start, InitialVelocity, GravityZ, Time);
			DrawDebugLine(World, PreviousPoint, Point, Color, false, 0.05f, 0, 6.0f);
			PreviousPoint = Point;
		}
		DrawDebugSphere(World, PreviousPoint, 55.0f, 16, Color, false, 0.05f, 0, 5.0f);
	}
}

AVectorPhysicsBoss::AVectorPhysicsBoss(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Archetype = EVectorEnemyArchetype::HeavyRhinoBeetle;
	MoveSpeedVarianceRatio = 0.0;
	MoveSpeedCmPerSecond = 150.0;
	GetCapsuleComponent()->InitCapsuleSize(105.0f, 120.0f);
	BossPhaseLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BossPhaseLight"));
	BossPhaseLight->SetupAttachment(GetCapsuleComponent());
	BossPhaseLight->SetRelativeLocation(FVector(0.0, 0.0, 110.0));
	BossPhaseLight->SetCastShadows(false);
	BossPhaseLight->SetVisibility(true);
	ExposedCoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExposedCore"));
	ExposedCoreMesh->SetupAttachment(GetCapsuleComponent());
	ExposedCoreMesh->SetRelativeLocation(FVector(0.0, 0.0, 72.0));
	ExposedCoreMesh->SetRelativeScale3D(FVector(0.72));
	ExposedCoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExposedCoreMesh->SetGenerateOverlapEvents(false);
	ExposedCoreMesh->SetCanEverAffectNavigation(false);
	ExposedCoreMesh->SetCastShadow(false);
	ExposedCoreMesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoreSphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CoreSphereMesh.Succeeded())
	{
		ExposedCoreMesh->SetStaticMesh(CoreSphereMesh.Object);
	}

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
	if (BreakableAnchorComponent)
	{
		BreakableAnchorComponent->MinimumClosingSpeedCmPerSecond = 720.0;
		BreakableAnchorComponent->MinimumLateralAlignment = 0.50;
		BreakableAnchorComponent->AnchoredPhysicalMass = 8.0;
		BreakableAnchorComponent->UnstablePhysicalMass = 4.0;
		BreakableAnchorComponent->LaunchablePhysicalMass = 3.0;
		BreakableAnchorComponent->AnchoredGroundFriction = 1.6;
		BreakableAnchorComponent->UnstableGroundFriction = 0.8;
		BreakableAnchorComponent->LaunchableGroundFriction = 0.3;
	}
	if (LeftAnchorMesh && RightAnchorMesh)
	{
		LeftAnchorMesh->SetRelativeLocation(FVector(0.0, -145.0, 0.0));
		RightAnchorMesh->SetRelativeLocation(FVector(0.0, 145.0, 0.0));
		LeftAnchorMesh->SetRelativeScale3D(FVector(0.45, 0.24, 1.35));
		RightAnchorMesh->SetRelativeScale3D(FVector(0.45, 0.24, 1.35));
	}
	PrototypeMeshOverride = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Vector/Art/PrototypeMonsters/Dragon/SM_Prototype_Dragon.SM_Prototype_Dragon")));
	PrototypeMeshScaleOverride = FVector(0.85);
	KineticOrbClass = AVectorKineticOrb::StaticClass();
}

void AVectorPhysicsBoss::BeginPlay()
{
	Super::BeginPlay();
	if (ExposedCoreMesh)
	{
		CoreMaterial = ExposedCoreMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (CoreMaterial)
		{
			CoreMaterial->SetVectorParameterValue(
				TEXT("Color"), FLinearColor(1.0f, 0.03f, 0.01f));
		}
	}
	const FVector BossSpawnLocation = GetActorLocation();
	ConfigureEncounterVoidRecovery(
		BossSpawnLocation.Z - FMath::Max(100.0, VoidRecoveryDepthBelowSpawnCm),
		BossSpawnLocation);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss void contract configured: actor=%s source=LOCAL_FALLBACK floorZ=%.0f drop=%s outcome=ENVIRONMENTAL_KILL check=PASS"),
		*GetName(), BossSpawnLocation.Z - FMath::Max(100.0, VoidRecoveryDepthBelowSpawnCm),
		*BossSpawnLocation.ToCompactString());
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
	if (BreakableAnchorComponent)
	{
		BreakableAnchorComponent->OnAnchorGroupBroken.AddUObject(
			this, &AVectorPhysicsBoss::HandleShellGroupBroken);
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
	const double SafeDeltaSeconds = FMath::Max(0.0, static_cast<double>(DeltaSeconds));
	const bool bResolveWasActive = BossState.IsStaggerResolveActive();
	BossState.AdvanceStaggerResolve(SafeDeltaSeconds);
	if (BossState.IsStaggerResolveActive())
	{
		// Resolve is not physics immunity: velocity and collision response remain intact.
		// It only prevents stability shots from opening another action-cancel window.
		if (StabilityComponent)
		{
			StabilityComponent->ResetStability();
		}
		if (BossPhaseLight)
		{
			BossPhaseLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.05f));
			BossPhaseLight->SetIntensity(9000.0f);
		}
		if (GetWorld())
		{
			DrawDebugCircle(GetWorld(), GetActorLocation() + FVector(0.0, 0.0, 12.0),
				190.0, 32, FColor::Yellow, false, 0.05f, 0, 7.0f,
				FVector::ForwardVector, FVector::RightVector, false);
		}
	}
	else if (bResolveWasActive)
	{
		if (StabilityComponent)
		{
			StabilityComponent->ResetStability();
		}
		UpdateBossPresentation();
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss stagger resolve expired: actor=%s nextInterrupt=AVAILABLE check=PASS"),
			*GetName());
	}
	MaintainKineticOrbSupply(SafeDeltaSeconds);
	AdvanceRam(SafeDeltaSeconds);
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
	const bool bAccepted = BossState.TryBeginStaggerResolve(
		FMath::Max(0.1, StaggerResolveSeconds));
	if (StabilityComponent)
	{
		StabilityComponent->ResetStability();
	}
	if (!bAccepted)
	{
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss stagger rejected: actor=%s reason=RESOLVE_ACTIVE remaining=%.2fs check=PASS"),
			*GetName(), BossState.GetStaggerResolveSecondsRemaining());
		return;
	}

	SetAttackWarningPresentation(false);
	ClearAmmoTargetPresentation();
	RamPhase = ERamPhase::Recovery;
	RamPhaseSecondsRemaining = FMath::Max(0.01, StaggerReactionSeconds);
	bStaggerCounterBurstPending = true;
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss stagger accepted: actor=%s reaction=%.2fs resolve=%.2fs counter=AERIAL_BURST shellBypass=no check=PASS"),
		*GetName(), RamPhaseSecondsRemaining,
		BossState.GetStaggerResolveSecondsRemaining());
}

void AVectorPhysicsBoss::HandleShellGroupBroken(
	const EVectorAnchorGroupSide Side,
	const int32 BrokenGroupCount)
{
	const EVectorPhysicsBossPhase PreviousPhase = BossState.GetPhase();
	if (BossState.NotifyStructureBroken(BrokenGroupCount))
	{
		ApplyPhaseOutputs(PreviousPhase);
	}
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss shell structure: actor=%s side=%s broken=%d/2 previous=%d current=%d coreExposed=%s"),
		*GetName(), Side == EVectorAnchorGroupSide::Right ? TEXT("RIGHT") : TEXT("LEFT"),
		BrokenGroupCount, static_cast<int32>(PreviousPhase),
		static_cast<int32>(BossState.GetPhase()),
		BrokenGroupCount >= 2 ? TEXT("YES") : TEXT("no"));
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
	if (RamPhase == ERamPhase::AerialBurstTelegraph && GetWorld())
	{
		const FVector Center = GetActorLocation() + FVector(0.0, 0.0, 8.0);
		DrawDebugCircle(GetWorld(), Center, AerialBurstRadiusCm,
			48, FColor::Orange, false, 0.05f, 0, 10.0f,
			FVector::ForwardVector, FVector::RightVector, false);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const double Angle = UE_TWO_PI * static_cast<double>(Index) / 8.0;
			const FVector Base = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0)
				* (AerialBurstRadiusCm * 0.65);
			DrawDebugDirectionalArrow(GetWorld(), Base, Base + FVector::UpVector * 360.0,
				24.0f, FColor::Orange, false, 0.05f, 0, 6.0f);
		}
	}
	if (RamPhase == ERamPhase::AmmoLaunchTelegraph && LockedAmmoTarget.IsValid())
	{
		FVector LaunchVelocity = FVector::ZeroVector;
		if (ComputeAmmoLaunchVelocity(LockedAmmoTarget.Get(), LaunchVelocity))
		{
			const UVectorCharacterMovementComponent* Movement =
				LockedAmmoTarget->FindComponentByClass<UVectorCharacterMovementComponent>();
			DrawBallisticArc(GetWorld(), LockedAmmoTarget->GetActorLocation(), LaunchVelocity,
				Movement ? Movement->GetGravityZ() : -980.0,
				AmmoLaunchFlightSeconds, FColor::Red);
		}
	}
	else if (RamPhase == ERamPhase::AmmoLaunchTelegraph && GetWorld())
	{
		const FVector Start = GetActorLocation() + FVector(0.0, 0.0, 70.0);
		FVector Direction = (LockedAmmoAimPoint - Start).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = GetActorForwardVector().GetSafeNormal2D();
		}
		DrawDebugDirectionalArrow(GetWorld(), Start,
			Start + Direction * 850.0, 85.0f, FColor::Cyan,
			false, 0.05f, 0, 8.0f);
		DrawDebugSphere(GetWorld(), Start + Direction * 180.0,
			45.0f, 16, FColor::Cyan, false, 0.05f, 0, 5.0f);
	}
	if (RamPhase == ERamPhase::SlamAirborne)
	{
		const UCharacterMovementComponent* BossMovement = GetCharacterMovement();
		const bool bHasLeftGround = RamPhaseSecondsRemaining
			< FMath::Max(0.1, SlamMaximumAirborneSeconds) - 0.1;
		if (bHasLeftGround && BossMovement && BossMovement->IsMovingOnGround())
		{
			RamPhase = ERamPhase::Recovery;
			RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
			UE_LOG(LogVectorBoss, Log,
				TEXT("Boss slam landed: actor=%s recovery=%.2fs (landing shock uses unified impact component)"),
				*GetName(), RamPhaseSecondsRemaining);
			return;
		}
	}

	if (StabilityComponent && StabilityComponent->IsStaggered()
		&& !BossState.IsStaggerResolveActive())
	{
		if (RamPhase == ERamPhase::Telegraph
			|| RamPhase == ERamPhase::SlamTelegraph
			|| RamPhase == ERamPhase::AerialBurstTelegraph
			|| RamPhase == ERamPhase::AmmoLaunchTelegraph)
		{
			SetAttackWarningPresentation(false);
			ClearAmmoTargetPresentation();
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
				BeginNextAttack();
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
	case ERamPhase::AerialBurstTelegraph:
		ReleaseAerialBurst();
		break;
	case ERamPhase::AmmoLaunchTelegraph:
		LaunchAmmoTarget();
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
		if (bStaggerCounterBurstPending)
		{
			bStaggerCounterBurstPending = false;
			UE_LOG(LogVectorBoss, Log,
				TEXT("Boss stagger counter begins: actor=%s attack=AERIAL_BURST resolveRemaining=%.2fs"),
				*GetName(), BossState.GetStaggerResolveSecondsRemaining());
			BeginAerialBurstTelegraph();
			break;
		}
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
		bQueued = Movement->QueueDirectionalAirborneVelocityOverride(
			FVector::UpVector, SlamLaunchSpeedCmPerSecond);
	}
	RamPhase = bQueued ? ERamPhase::SlamAirborne : ERamPhase::Recovery;
	RamPhaseSecondsRemaining = bQueued
		? FMath::Max(0.1, SlamMaximumAirborneSeconds)
		: BossState.GetRecoverySeconds();
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss slam launch: actor=%s queued=%s verticalSpeed=%.0f mode=%s"),
		*GetName(), bQueued ? TEXT("OK") : TEXT("REJECTED"),
		SlamLaunchSpeedCmPerSecond, bQueued ? TEXT("PendingLaunch") : TEXT("Recovery"));
}

void AVectorPhysicsBoss::BeginNextAttack()
{
	const bool bAmmoAvailable = KineticOrbClass != nullptr || FindAmmoTarget() != nullptr;
	const EVectorPhysicsBossAttack Attack = BossState.SelectAttack(
		AttackSequenceIndex++, bAmmoAvailable);
	switch (Attack)
	{
	case EVectorPhysicsBossAttack::Ram:
		BeginRamTelegraph();
		break;
	case EVectorPhysicsBossAttack::Slam:
		BeginSlamTelegraph();
		break;
	case EVectorPhysicsBossAttack::AerialBurst:
		BeginAerialBurstTelegraph();
		break;
	case EVectorPhysicsBossAttack::AmmoLaunch:
		if (!BeginAmmoLaunchTelegraph())
		{
			BeginAerialBurstTelegraph();
		}
		break;
	case EVectorPhysicsBossAttack::None:
	default:
		RamPhase = ERamPhase::Recovery;
		RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
		break;
	}
}

void AVectorPhysicsBoss::BeginAerialBurstTelegraph()
{
	RamPhase = ERamPhase::AerialBurstTelegraph;
	RamPhaseSecondsRemaining = FMath::Max(0.01, AerialBurstTelegraphSeconds);
	SetAttackWarningPresentation(true);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss aerial burst telegraph: actor=%s duration=%.2fs radius=%.0f verticalBase=%.0f"),
		*GetName(), RamPhaseSecondsRemaining, AerialBurstRadiusCm,
		AerialBurstVerticalBaseSpeedCmPerSecond);
}

void AVectorPhysicsBoss::ReleaseAerialBurst()
{
	SetAttackWarningPresentation(false);
	int32 AffectedCount = 0;
	for (TActorIterator<ACharacter> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ACharacter* Target = *Iterator;
		if (!Target || Target == this
			|| FVector::Dist2D(Target->GetActorLocation(), GetActorLocation())
				> FMath::Max(0.0, AerialBurstRadiusCm)
			|| FMath::Abs(Target->GetActorLocation().Z - GetActorLocation().Z) > 800.0)
		{
			continue;
		}
		if (const UVectorHealthComponent* Health =
			Target->FindComponentByClass<UVectorHealthComponent>(); Health && Health->IsDead())
		{
			continue;
		}
		UVectorCharacterMovementComponent* Movement =
			Target->FindComponentByClass<UVectorCharacterMovementComponent>();
		if (!Movement)
		{
			continue;
		}
		const UVectorStabilityComponent* Stability =
			Target->FindComponentByClass<UVectorStabilityComponent>();
		const double Mass = Stability ? Stability->GetEffectivePhysicalMass() : 2.5;
		FVector OutwardDirection =
			(Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (OutwardDirection.IsNearlyZero())
		{
			OutwardDirection = GetActorForwardVector().GetSafeNormal2D();
		}
		const double HorizontalSpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(
			AerialBurstHorizontalBaseSpeedCmPerSecond, Mass);
		const double VerticalSpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(
			AerialBurstVerticalBaseSpeedCmPerSecond, Mass);
		const FVector LaunchVelocity =
			OutwardDirection * HorizontalSpeed + FVector::UpVector * VerticalSpeed;
		const bool bQueued = Movement->QueueAirborneWorldVelocityOverride(LaunchVelocity);
		if (bQueued)
		{
			++AffectedCount;
		}
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss aerial burst target: target=%s mass=%.2f velocity=%s queued=%s"),
			*Target->GetName(), Mass, *LaunchVelocity.ToCompactString(),
			bQueued ? TEXT("OK") : TEXT("REJECTED"));
	}
	RamPhase = ERamPhase::Recovery;
	RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss aerial burst released: actor=%s affected=%d recovery=%.2fs"),
		*GetName(), AffectedCount, RamPhaseSecondsRemaining);
}

bool AVectorPhysicsBoss::BeginAmmoLaunchTelegraph()
{
	AVectorEnemy* AmmoTarget = FindAmmoTarget();
	APawn* PlayerPawn = FindPlayerPawn();
	if (!PlayerPawn || (!AmmoTarget && !KineticOrbClass))
	{
		return false;
	}
	LockedAmmoTarget = AmmoTarget;
	LockedAmmoAimPoint = PlayerPawn->GetActorLocation();
	if (AmmoTarget)
	{
		AmmoTarget->SetAttackWarningPresentation(true);
	}
	SetAttackWarningPresentation(true);
	RamPhase = ERamPhase::AmmoLaunchTelegraph;
	RamPhaseSecondsRemaining = FMath::Max(0.01, AmmoLaunchTelegraphSeconds);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss ammo telegraph: boss=%s mode=%s ammo=%s aim=%s duration=%.2fs"),
		*GetName(), AmmoTarget ? TEXT("LIVE_ENEMY") : TEXT("KINETIC_ORB"),
		*GetNameSafe(AmmoTarget), *LockedAmmoAimPoint.ToCompactString(),
		RamPhaseSecondsRemaining);
	return true;
}

void AVectorPhysicsBoss::LaunchAmmoTarget()
{
	SetAttackWarningPresentation(false);
	AVectorEnemy* AmmoTarget = LockedAmmoTarget.Get();
	if (!AmmoTarget)
	{
		const bool bSpawned = SpawnKineticOrb(true);
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss reusable ammo release: boss=%s spawned=%s activeCap=%d"),
			*GetName(), bSpawned ? TEXT("YES") : TEXT("no"),
			MaximumActiveKineticOrbs);
		ClearAmmoTargetPresentation();
		RamPhase = ERamPhase::Recovery;
		RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
		return;
	}
	UVectorStabilityComponent* Stability = AmmoTarget
		? AmmoTarget->FindComponentByClass<UVectorStabilityComponent>() : nullptr;
	const bool bDisarmed = !AmmoTarget
		|| (Stability && Stability->IsStaggered())
		|| FVector::Dist(AmmoTarget->GetActorLocation(), GetActorLocation())
			> FMath::Max(0.0, AmmoSearchRadiusCm) * 1.25;
	FVector LaunchVelocity = FVector::ZeroVector;
	if (bDisarmed || !ComputeAmmoLaunchVelocity(AmmoTarget, LaunchVelocity))
	{
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss ammo arc disarmed: boss=%s ammo=%s reason=%s"),
			*GetName(), *GetNameSafe(AmmoTarget),
			bDisarmed ? TEXT("dead/staggered/displaced") : TEXT("invalid ballistic solve"));
		ClearAmmoTargetPresentation();
		RamPhase = ERamPhase::Recovery;
		RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
		return;
	}
	UVectorCharacterMovementComponent* Movement =
		AmmoTarget->FindComponentByClass<UVectorCharacterMovementComponent>();
	const bool bQueued = Movement
		&& Movement->QueueAirborneWorldVelocityOverride(LaunchVelocity);
	const FVector PredictedLanding = FVectorImpactMath::SampleBallisticPosition(
		AmmoTarget->GetActorLocation(), LaunchVelocity,
		Movement ? Movement->GetGravityZ() : -980.0,
		AmmoLaunchFlightSeconds);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss ammo arc launch: boss=%s ammo=%s velocity=%s flight=%.2fs predicted=%s locked=%s error=%.0f queued=%s"),
		*GetName(), *AmmoTarget->GetName(), *LaunchVelocity.ToCompactString(),
		AmmoLaunchFlightSeconds, *PredictedLanding.ToCompactString(),
		*LockedAmmoAimPoint.ToCompactString(),
		FVector::Dist(PredictedLanding, LockedAmmoAimPoint),
		bQueued ? TEXT("OK") : TEXT("REJECTED"));
	ClearAmmoTargetPresentation();
	RamPhase = ERamPhase::Recovery;
	RamPhaseSecondsRemaining = BossState.GetRecoverySeconds();
}

int32 AVectorPhysicsBoss::CountActiveKineticOrbs() const
{
	int32 ActiveCount = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVectorKineticOrb> Iterator(World); Iterator; ++Iterator)
		{
			const AVectorKineticOrb* Orb = *Iterator;
			if (IsValid(Orb) && Orb->GetOwner() == this)
			{
				++ActiveCount;
			}
		}
	}
	return ActiveCount;
}

void AVectorPhysicsBoss::MaintainKineticOrbSupply(const double DeltaSeconds)
{
	if (!KineticOrbClass || BossState.IsDefeated()
		|| MinimumAvailableKineticOrbs <= 0)
	{
		return;
	}
	KineticOrbSupplySecondsRemaining -= FMath::Max(0.0, DeltaSeconds);
	if (KineticOrbSupplySecondsRemaining > 0.0)
	{
		return;
	}
	KineticOrbSupplySecondsRemaining = FMath::Max(0.1, KineticOrbSupplyIntervalSeconds);
	const int32 ActiveCount = CountActiveKineticOrbs();
	if (ActiveCount >= FMath::Max(0, MinimumAvailableKineticOrbs))
	{
		return;
	}
	const bool bSpawned = SpawnKineticOrb(false);
	UE_LOG(LogVectorBoss, Log,
		TEXT("Boss fallback ammo supply: boss=%s activeBefore=%d minimum=%d spawned=%s role=SHIELD_BREAK_AMMO check=%s"),
		*GetName(), ActiveCount, MinimumAvailableKineticOrbs,
		bSpawned ? TEXT("YES") : TEXT("no"), bSpawned ? TEXT("PASS") : TEXT("FAIL"));
}

bool AVectorPhysicsBoss::SpawnKineticOrb(const bool bLaunchTowardPlayer)
{
	UWorld* World = GetWorld();
	if (!World || !KineticOrbClass)
	{
		return false;
	}
	int32 ActiveCount = 0;
	AVectorKineticOrb* OldestOrb = nullptr;
	double OldestAge = -1.0;
	for (TActorIterator<AVectorKineticOrb> Iterator(World); Iterator; ++Iterator)
	{
		AVectorKineticOrb* Orb = *Iterator;
		if (!IsValid(Orb) || Orb->GetOwner() != this)
		{
			continue;
		}
		++ActiveCount;
		const double Age = Orb->GetGameTimeSinceCreation();
		if (Age > OldestAge)
		{
			OldestAge = Age;
			OldestOrb = Orb;
		}
	}
	if (ActiveCount >= FMath::Max(1, MaximumActiveKineticOrbs) && OldestOrb)
	{
		UE_LOG(LogVectorBoss, Log,
			TEXT("Boss orb budget recycles oldest: boss=%s orb=%s age=%.1f active=%d cap=%d"),
			*GetName(), *OldestOrb->GetName(), OldestAge, ActiveCount,
			MaximumActiveKineticOrbs);
		OldestOrb->Destroy();
	}

	FVector Direction = (LockedAmmoAimPoint - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
	}
	FVector SpawnLocation = GetActorLocation()
		+ Direction * 190.0 + FVector(0.0, 0.0, 55.0);
	if (!bLaunchTowardPlayer)
	{
		const FVector SideDirection(-Direction.Y, Direction.X, 0.0);
		const double SideSign = (AttackSequenceIndex % 2 == 0) ? 1.0 : -1.0;
		SpawnLocation = GetActorLocation() + Direction * 80.0
			+ SideDirection * (300.0 * SideSign) + FVector(0.0, 0.0, 55.0);
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AVectorKineticOrb* Orb = World->SpawnActor<AVectorKineticOrb>(
		KineticOrbClass, SpawnLocation, Direction.Rotation(), Parameters);
	if (!Orb)
	{
		return false;
	}
	return bLaunchTowardPlayer
		? Orb->Launch(Direction, KineticOrbLaunchSpeedCmPerSecond, this)
		: Orb->Arm(this);
}

void AVectorPhysicsBoss::ClearAmmoTargetPresentation()
{
	if (LockedAmmoTarget.IsValid())
	{
		LockedAmmoTarget->SetAttackWarningPresentation(false);
	}
	LockedAmmoTarget.Reset();
	LockedAmmoAimPoint = FVector::ZeroVector;
}

AVectorEnemy* AVectorPhysicsBoss::FindAmmoTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	AVectorEnemy* BestTarget = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (TActorIterator<AVectorEnemy> Iterator(World); Iterator; ++Iterator)
	{
		AVectorEnemy* Candidate = *Iterator;
		if (!Candidate || Candidate == this || Candidate->IsLethalLaunchCorpse())
		{
			continue;
		}
		const UVectorHealthComponent* Health =
			Candidate->FindComponentByClass<UVectorHealthComponent>();
		const double DistanceSquared = FVector::DistSquared(
			Candidate->GetActorLocation(), GetActorLocation());
		if (!Health || Health->IsDead()
			|| DistanceSquared > FMath::Square(FMath::Max(0.0, AmmoSearchRadiusCm))
			|| DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}
		BestDistanceSquared = DistanceSquared;
		BestTarget = Candidate;
	}
	return BestTarget;
}

bool AVectorPhysicsBoss::ComputeAmmoLaunchVelocity(
	AVectorEnemy* AmmoTarget,
	FVector& OutVelocity) const
{
	OutVelocity = FVector::ZeroVector;
	UVectorCharacterMovementComponent* Movement = AmmoTarget
		? AmmoTarget->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	const UVectorStabilityComponent* Stability = AmmoTarget
		? AmmoTarget->FindComponentByClass<UVectorStabilityComponent>() : nullptr;
	if (!Movement || !Stability
		|| !FVectorImpactMath::ComputeBallisticLaunchVelocity(
			AmmoTarget->GetActorLocation(), LockedAmmoAimPoint,
			FMath::Max(0.1, AmmoLaunchFlightSeconds), Movement->GetGravityZ(), OutVelocity))
	{
		return false;
	}
	const double MaximumSpeed = FVectorImpactMath::ComputeMassAdjustedSpeed(
		AmmoLaunchMaximumBaseSpeedCmPerSecond, Stability->GetEffectivePhysicalMass());
	if (MaximumSpeed <= 0.0)
	{
		return false;
	}
	OutVelocity = OutVelocity.GetClampedToMaxSize(MaximumSpeed);
	return !OutVelocity.IsNearlyZero();
}

void AVectorPhysicsBoss::ApplyPhaseOutputs(const EVectorPhysicsBossPhase PreviousPhase)
{
	if (StabilityComponent)
	{
		const double PhaseMass = BossState.GetEffectivePhysicalMass();
		StabilityComponent->PhysicalMassHeavy = PhaseMass;
		StabilityComponent->StaggeredPhysicalMassHeavy = PhaseMass;
	}
	MoveSpeedCmPerSecond = BossState.GetPursuitSpeedCmPerSecond();
	if (UCharacterMovementComponent* BossMovement = GetCharacterMovement())
	{
		BossMovement->MaxWalkSpeed = static_cast<float>(MoveSpeedCmPerSecond);
	}
	UpdateBossPresentation();
	const EVectorPhysicsBossPhase CurrentPhase = BossState.GetPhase();
	if (CurrentPhase == EVectorPhysicsBossPhase::Defeated)
	{
		SetAttackWarningPresentation(false);
		ClearAmmoTargetPresentation();
		RamPhase = ERamPhase::Waiting;
		RamPhaseSecondsRemaining = 0.0;
	}
	if (CurrentPhase != PreviousPhase)
	{
		OnBossPhaseChanged.Broadcast(PreviousPhase, CurrentPhase);
	}
	UE_LOG(LogVectorBoss, Log, TEXT("Boss phase applied: actor=%s previous=%d current=%d pursuit=%.0f engagementRange=%.0f %s"),
		*GetName(), static_cast<int32>(PreviousPhase), static_cast<int32>(CurrentPhase),
		MoveSpeedCmPerSecond, RamTriggerRangeCm, *BossState.Describe());
}

void AVectorPhysicsBoss::UpdateBossPresentation()
{
	FLinearColor Color(0.45f, 0.12f, 0.65f);
	float PhaseLightIntensity = 2600.0f;
	const bool bUsingPrototypeMesh = BodyMesh
		&& BodyMesh->GetStaticMesh()
		&& BodyMesh->GetStaticMesh()->GetName() == TEXT("SM_Prototype_Dragon");
	FVector Scale = bUsingPrototypeMesh
		? PrototypeMeshScaleOverride
		: FVector(2.1f, 2.1f, 2.4f);
	switch (BossState.GetPhase())
	{
	case EVectorPhysicsBossPhase::ExposedShell:
		Color = FLinearColor(1.0f, 0.45f, 0.06f);
		PhaseLightIntensity = 4800.0f;
		Scale *= 0.94f;
		break;
	case EVectorPhysicsBossPhase::Overload:
		Color = FLinearColor(1.0f, 0.05f, 0.03f);
		PhaseLightIntensity = 7600.0f;
		Scale *= 0.90f;
		break;
	case EVectorPhysicsBossPhase::Defeated:
		Color = FLinearColor(0.1f, 0.1f, 0.1f);
		PhaseLightIntensity = 0.0f;
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
		if (!bUsingPrototypeMesh)
		{
			BodyMesh->SetRelativeLocation(FVector(0.0, 0.0,
				-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 50.0 * Scale.Z));
		}
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	if (BossPhaseLight)
	{
		BossPhaseLight->SetLightColor(Color);
		BossPhaseLight->SetIntensity(PhaseLightIntensity);
		BossPhaseLight->SetAttenuationRadius(760.0f);
	}
	if (ExposedCoreMesh)
	{
		const bool bCoreVisible = BossState.GetPhase() == EVectorPhysicsBossPhase::Overload;
		ExposedCoreMesh->SetVisibility(bCoreVisible);
		ExposedCoreMesh->SetRelativeScale3D(bCoreVisible ? FVector(0.72) : FVector(0.01));
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
