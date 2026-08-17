// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boss/VectorKineticOrb.h"

#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stability/VectorStabilityComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorKineticOrb, Log, All);

FVector FVectorWeakGuidanceMath::TurnDirection(
	const FVector& CurrentDirection,
	const FVector& DesiredDirection,
	const double MaximumTurnRateDegreesPerSecond,
	const double DeltaSeconds)
{
	const FVector SafeCurrent = CurrentDirection.GetSafeNormal2D();
	const FVector SafeDesired = DesiredDirection.GetSafeNormal2D();
	if (SafeCurrent.IsNearlyZero() || SafeDesired.IsNearlyZero()
		|| !FMath::IsFinite(MaximumTurnRateDegreesPerSecond)
		|| !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
	{
		return SafeCurrent;
	}
	const FRotator GuidedRotation = FMath::RInterpConstantTo(
		SafeCurrent.Rotation(), SafeDesired.Rotation(),
		static_cast<float>(DeltaSeconds),
		static_cast<float>(FMath::Max(0.0, MaximumTurnRateDegreesPerSecond)));
	return GuidedRotation.Vector().GetSafeNormal2D();
}

AVectorKineticOrb::AVectorKineticOrb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	MassClass = EVectorMassClass::Light;
	GetCapsuleComponent()->InitCapsuleSize(38.0f, 38.0f);
	GetCharacterMovement()->GroundFriction = 0.12f;
	GetCharacterMovement()->BrakingFrictionFactor = 0.2f;
	GetCharacterMovement()->BrakingDecelerationWalking = 45.0f;
	GetCharacterMovement()->MaxWalkSpeed = 3200.0f;
	if (StabilityComponent)
	{
		StabilityComponent->MassClass = EVectorMassClass::Light;
		StabilityComponent->PhysicalMassLight = 1.0;
		StabilityComponent->StaggeredPhysicalMassLight = 1.0;
	}
	if (HealthComponent)
	{
		HealthComponent->MaxHealth = 35.0;
	}
	if (ImpactCollisionComponent)
	{
		ImpactCollisionComponent->MinDamageSpeedCmPerSecond = 260.0;
		ImpactCollisionComponent->MaxDamage = 50.0;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded() && BodyMesh)
	{
		BodyMesh->SetStaticMesh(SphereMeshFinder.Object);
	}
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AVectorKineticOrb::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GuidanceSecondsRemaining <= 0.0)
	{
		return;
	}
	GuidanceSecondsRemaining = FMath::Max(
		0.0, GuidanceSecondsRemaining - FMath::Max(0.0f, DeltaSeconds));
	APawn* TargetPawn = GuidanceTarget.Get();
	if (!TargetPawn || GuidanceSecondsRemaining <= 0.0)
	{
		DisarmGuidance(TargetPawn ? TEXT("TIMEOUT") : TEXT("TARGET_LOST"));
		return;
	}
	GuidanceUpdateAccumulatorSeconds += FMath::Max(0.0f, DeltaSeconds);
	if (GuidanceUpdateAccumulatorSeconds < 0.12)
	{
		return;
	}
	const double GuidanceStepSeconds = GuidanceUpdateAccumulatorSeconds;
	GuidanceUpdateAccumulatorSeconds = 0.0;

	const FVector DesiredDirection =
		(TargetPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}
	GuidanceDirection = FVectorWeakGuidanceMath::TurnDirection(
		GuidanceDirection, DesiredDirection,
		GuidanceTurnRateDegreesPerSecond, GuidanceStepSeconds);
	if (UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		Movement->QueueDirectionalVelocityOverride(
			GuidanceDirection, GuidanceSpeedCmPerSecond);
	}
}

void AVectorKineticOrb::BeginPlay()
{
	Super::BeginPlay();
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AVectorKineticOrb::HandleOrbDeath);
	}
	BaseBodyScale = FVector(0.78f);
	BaseBodyColor = FLinearColor(0.1f, 0.8f, 1.0f);
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(BaseBodyScale);
		BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, -38.0));
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), BaseBodyColor);
	}
	if (LiftForkLight)
	{
		LiftForkLight->SetLightColor(BaseBodyColor);
		LiftForkLight->SetIntensity(3200.0f);
		LiftForkLight->SetAttenuationRadius(360.0f);
	}
}

bool AVectorKineticOrb::Launch(
	const FVector& Direction,
	const double SpeedCmPerSecond,
	AActor* SourceActor)
{
	const FVector SafeDirection = Direction.GetSafeNormal2D();
	if (SafeDirection.IsNearlyZero() || !FMath::IsFinite(SpeedCmPerSecond)
		|| SpeedCmPerSecond <= 0.0)
	{
		return false;
	}
	Arm(SourceActor);
	UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>();
	const bool bQueued = Movement
		&& Movement->QueueDirectionalVelocityOverride(SafeDirection, SpeedCmPerSecond);
	UE_LOG(LogVectorKineticOrb, Log,
		TEXT("Kinetic orb launch: orb=%s source=%s direction=%s speed=%.0f queued=%s lifetime=%.1f"),
		*GetName(), *GetNameSafe(SourceActor), *SafeDirection.ToCompactString(),
		SpeedCmPerSecond, bQueued ? TEXT("OK") : TEXT("REJECTED"),
		MaximumLifetimeSeconds);
	return bQueued;
}

bool AVectorKineticOrb::LaunchWeakHoming(
	APawn* TargetPawn,
	const double SpeedCmPerSecond,
	const double GuidanceSeconds,
	const double MaximumTurnRateDegreesPerSecond,
	AActor* SourceActor)
{
	if (!TargetPawn || !FMath::IsFinite(SpeedCmPerSecond)
		|| SpeedCmPerSecond <= 0.0 || !FMath::IsFinite(GuidanceSeconds)
		|| GuidanceSeconds <= 0.0)
	{
		return false;
	}
	GuidanceDirection =
		(TargetPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (GuidanceDirection.IsNearlyZero())
	{
		return false;
	}
	GuidanceTarget = TargetPawn;
	GuidanceSpeedCmPerSecond = SpeedCmPerSecond;
	GuidanceSecondsRemaining = GuidanceSeconds;
	GuidanceTurnRateDegreesPerSecond = FMath::Max(
		0.0, MaximumTurnRateDegreesPerSecond);
	GuidanceUpdateAccumulatorSeconds = 0.0;
	const bool bLaunched = Launch(GuidanceDirection, SpeedCmPerSecond, SourceActor);
	if (!bLaunched)
	{
		GuidanceSecondsRemaining = 0.0;
		GuidanceUpdateAccumulatorSeconds = 0.0;
		GuidanceTarget.Reset();
	}
	UE_LOG(LogVectorKineticOrb, Log,
		TEXT("Kinetic orb guidance armed: orb=%s target=%s speed=%.0f duration=%.2fs turnRate=%.0f launched=%s role=WEAK_HOMING"),
		*GetName(), *GetNameSafe(TargetPawn), SpeedCmPerSecond,
		GuidanceSecondsRemaining, GuidanceTurnRateDegreesPerSecond,
		bLaunched ? TEXT("YES") : TEXT("no"));
	return bLaunched;
}

void AVectorKineticOrb::DisarmGuidance(const TCHAR* Reason)
{
	if (GuidanceSecondsRemaining <= 0.0 && !GuidanceTarget.IsValid())
	{
		return;
	}
	GuidanceSecondsRemaining = 0.0;
	GuidanceUpdateAccumulatorSeconds = 0.0;
	GuidanceTarget.Reset();
	UE_LOG(LogVectorKineticOrb, Log,
		TEXT("Kinetic orb guidance disarmed: orb=%s reason=%s role=PLAYER_AMMO check=PASS"),
		*GetName(), Reason ? Reason : TEXT("UNKNOWN"));
}

void AVectorKineticOrb::ConfigureProjectilePresentation(
	const FLinearColor& Color,
	const double LifetimeSeconds)
{
	MaximumLifetimeSeconds = FMath::Max(0.1, LifetimeSeconds);
	BaseBodyColor = Color;
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), BaseBodyColor);
	}
	if (LiftForkLight)
	{
		LiftForkLight->SetLightColor(BaseBodyColor);
		LiftForkLight->SetIntensity(4200.0f);
		LiftForkLight->SetAttenuationRadius(420.0f);
	}
}

bool AVectorKineticOrb::Arm(AActor* SourceActor)
{
	SetOwner(SourceActor);
	SetLifeSpan(static_cast<float>(FMath::Max(0.1, MaximumLifetimeSeconds)));
	UE_LOG(LogVectorKineticOrb, Log,
		TEXT("Kinetic orb armed: orb=%s source=%s lifetime=%.1f role=PLAYER_REDIRECTABLE_AMMO"),
		*GetName(), *GetNameSafe(SourceActor), MaximumLifetimeSeconds);
	return true;
}

void AVectorKineticOrb::HandleOrbDeath()
{
	UE_LOG(LogVectorKineticOrb, Log, TEXT("Kinetic orb consumed: orb=%s"), *GetName());
	Destroy();
}
