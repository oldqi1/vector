// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/VectorEnvironmentalRedirector.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorRedirector, Log, All);

namespace VectorEnvironmentalRedirectorInternal
{
	bool IsFiniteVector(const FVector& Value)
	{
		return !Value.ContainsNaN()
			&& FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}
}

bool FVectorEnvironmentalRedirectResult::IsWithinInputBudget(
	const double Tolerance) const
{
	return bValid
		&& VectorEnvironmentalRedirectorInternal::IsFiniteVector(OutputVelocity)
		&& FMath::IsFinite(InputSpeedCmPerSecond)
		&& FMath::IsFinite(OutputSpeedCmPerSecond)
		&& OutputVelocity.Size() <= InputSpeedCmPerSecond + FMath::Max(0.0, Tolerance)
		&& OutputSpeedCmPerSecond <= InputSpeedCmPerSecond + FMath::Max(0.0, Tolerance);
}

FVectorEnvironmentalRedirectResult FVectorEnvironmentalRedirectMath::ComputeRedirect(
	const FVector& IncomingVelocity,
	const FVector& ExitDirection,
	const double Efficiency)
{
	FVectorEnvironmentalRedirectResult Result;
	if (!VectorEnvironmentalRedirectorInternal::IsFiniteVector(IncomingVelocity)
		|| !VectorEnvironmentalRedirectorInternal::IsFiniteVector(ExitDirection)
		|| !FMath::IsFinite(Efficiency)
		|| Efficiency < 0.0 || Efficiency > 1.0)
	{
		return Result;
	}

	const FVector SafeExitDirection = ExitDirection.GetSafeNormal();
	Result.InputSpeedCmPerSecond = IncomingVelocity.Size();
	if (SafeExitDirection.IsNearlyZero()
		|| Result.InputSpeedCmPerSecond <= UE_KINDA_SMALL_NUMBER)
	{
		return Result;
	}

	Result.OutputSpeedCmPerSecond = Result.InputSpeedCmPerSecond * Efficiency;
	Result.OutputVelocity = SafeExitDirection * Result.OutputSpeedCmPerSecond;
	Result.bValid = VectorEnvironmentalRedirectorInternal::IsFiniteVector(
		Result.OutputVelocity);
	if (!Result.IsWithinInputBudget())
	{
		Result.bValid = false;
	}
	return Result;
}

bool FVectorEnvironmentalRedirectMath::ShouldConsume(
	const bool bImpulseDriven,
	const bool bAlreadyConsumedThisOverlap,
	const double InputSpeedCmPerSecond,
	const double MinimumInputSpeedCmPerSecond)
{
	return bImpulseDriven
		&& !bAlreadyConsumedThisOverlap
		&& FMath::IsFinite(InputSpeedCmPerSecond)
		&& FMath::IsFinite(MinimumInputSpeedCmPerSecond)
		&& MinimumInputSpeedCmPerSecond >= 0.0
		&& InputSpeedCmPerSecond >= MinimumInputSpeedCmPerSecond;
}

AVectorEnvironmentalRedirector::AVectorEnvironmentalRedirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	TriggerBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBounds"));
	SetRootComponent(TriggerBounds);
	TriggerBounds->SetBoxExtent(FVector(210.0, 210.0, 150.0));
	TriggerBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBounds->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBounds->SetGenerateOverlapEvents(true);
	TriggerBounds->SetCanEverAffectNavigation(false);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(TriggerBounds);
	PadMesh->SetRelativeLocation(FVector(0.0, 0.0, -135.0));
	PadMesh->SetRelativeScale3D(FVector(4.2, 4.2, 0.12));
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetCanEverAffectNavigation(false);

	ExitMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMarkerMesh"));
	ExitMarkerMesh->SetupAttachment(TriggerBounds);
	ExitMarkerMesh->SetRelativeLocation(FVector(205.0, 0.0, 25.0));
	ExitMarkerMesh->SetRelativeScale3D(FVector(3.2, 0.18, 0.18));
	ExitMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExitMarkerMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeMesh.Object);
		ExitMarkerMesh->SetStaticMesh(CubeMesh.Object);
	}

	ExitArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ExitArrow"));
	ExitArrow->SetupAttachment(TriggerBounds);
	ExitArrow->SetRelativeLocation(FVector(0.0, 0.0, 30.0));
	ExitArrow->ArrowColor = FColor(40, 255, 230);
	ExitArrow->ArrowSize = 5.0f;
	ExitArrow->SetHiddenInGame(false);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(TriggerBounds);
	StatusLight->SetRelativeLocation(FVector(0.0, 0.0, 80.0));
	StatusLight->SetLightColor(FLinearColor(0.05f, 1.0f, 0.75f));
	StatusLight->SetIntensity(5200.0f);
	StatusLight->SetAttenuationRadius(520.0f);
	StatusLight->SetCastShadows(false);
}

void AVectorEnvironmentalRedirector::BeginPlay()
{
	Super::BeginPlay();
	TriggerBounds->OnComponentBeginOverlap.AddDynamic(
		this, &AVectorEnvironmentalRedirector::HandleBeginOverlap);
	TriggerBounds->OnComponentEndOverlap.AddDynamic(
		this, &AVectorEnvironmentalRedirector::HandleEndOverlap);

	TArray<AActor*> InitiallyOverlappingActors;
	TriggerBounds->GetOverlappingActors(InitiallyOverlappingActors);
	for (AActor* Actor : InitiallyOverlappingActors)
	{
		TryRedirect(Actor);
	}
}

void AVectorEnvironmentalRedirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bDrawDebug || !GetWorld() || !TriggerBounds)
	{
		return;
	}

	const FVector Origin = TriggerBounds->GetComponentLocation();
	const FVector ExitDirection = GetActorForwardVector().GetSafeNormal();
	DrawDebugBox(GetWorld(), Origin, TriggerBounds->GetScaledBoxExtent(),
		TriggerBounds->GetComponentQuat(), FColor::Cyan, false, 0.04f, 0, 3.0f);
	DrawDebugDirectionalArrow(GetWorld(), Origin + FVector::UpVector * 35.0,
		Origin + FVector::UpVector * 35.0 + ExitDirection * 500.0,
		90.0f, FColor::Cyan, false, 0.04f, 0, 8.0f);
}

void AVectorEnvironmentalRedirector::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryRedirect(OtherActor);
}

void AVectorEnvironmentalRedirector::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	if (!OtherActor || (TriggerBounds && TriggerBounds->IsOverlappingActor(OtherActor)))
	{
		return;
	}

	const int32 Removed = ConsumedActors.Remove(TWeakObjectPtr<AActor>(OtherActor));
	if (Removed > 0)
	{
		UE_LOG(LogVectorRedirector, Log,
			TEXT("Environment redirect reset: redirector=%s actor=%s reason=FULLY_EXITED check=PASS"),
			*GetName(), *OtherActor->GetName());
	}
}

void AVectorEnvironmentalRedirector::TryRedirect(AActor* OtherActor)
{
	if (!OtherActor)
	{
		return;
	}
	UVectorCharacterMovementComponent* Movement =
		OtherActor->FindComponentByClass<UVectorCharacterMovementComponent>();
	if (!Movement)
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(OtherActor);
	const FVector IncomingVelocity = Movement->GetEffectiveVelocityForPendingStep();
	const bool bAlreadyConsumed = ConsumedActors.Contains(ActorKey);
	if (!FVectorEnvironmentalRedirectMath::ShouldConsume(
		Movement->IsImpulseDriven(), bAlreadyConsumed, IncomingVelocity.Size(),
		MinimumInputSpeedCmPerSecond))
	{
		UE_LOG(LogVectorRedirector, Verbose,
			TEXT("Environment redirect ignored: redirector=%s actor=%s impulseDriven=%d consumed=%d speed=%.0f min=%.0f"),
			*GetName(), *OtherActor->GetName(), Movement->IsImpulseDriven() ? 1 : 0,
			bAlreadyConsumed ? 1 : 0, IncomingVelocity.Size(), MinimumInputSpeedCmPerSecond);
		return;
	}

	const FVector ExitDirection = GetActorForwardVector().GetSafeNormal();
	const FVectorEnvironmentalRedirectResult Result =
		FVectorEnvironmentalRedirectMath::ComputeRedirect(
			IncomingVelocity, ExitDirection, RedirectEfficiency);
	if (!Result.bValid || !Result.IsWithinInputBudget())
	{
		UE_LOG(LogVectorRedirector, Warning,
			TEXT("Environment redirect rejected: redirector=%s actor=%s input=%s exit=%s efficiency=%.2f budgetCheck=FAIL"),
			*GetName(), *OtherActor->GetName(), *IncomingVelocity.ToCompactString(),
			*ExitDirection.ToCompactString(), RedirectEfficiency);
		return;
	}

	const bool bNeedsAirborneTransport =
		!FMath::IsNearlyZero(Result.OutputVelocity.Z, 1.0);
	const bool bQueued = bNeedsAirborneTransport
		? Movement->QueueAirborneWorldVelocityOverride(Result.OutputVelocity)
		: Movement->QueueWorldVelocityOverride(Result.OutputVelocity);
	if (!bQueued)
	{
		UE_LOG(LogVectorRedirector, Warning,
			TEXT("Environment redirect queue failed: redirector=%s actor=%s output=%s transport=%s"),
			*GetName(), *OtherActor->GetName(), *Result.OutputVelocity.ToCompactString(),
			bNeedsAirborneTransport ? TEXT("AIRBORNE") : TEXT("GROUND"));
		return;
	}

	ConsumedActors.Add(ActorKey);
	Movement->BeginMomentumCarry(MomentumCarrySeconds);
	UE_LOG(LogVectorRedirector, Log,
		TEXT("Environment redirect: redirector=%s actor=%s input=%s inputSpeed=%.0f exit=%s output=%s outputSpeed=%.0f efficiency=%.2f impulseDriven=1 firstThisOverlap=1 transport=%s budgetCheck=PASS"),
		*GetName(), *OtherActor->GetName(), *IncomingVelocity.ToCompactString(),
		Result.InputSpeedCmPerSecond, *ExitDirection.ToCompactString(),
		*Result.OutputVelocity.ToCompactString(), Result.OutputSpeedCmPerSecond,
		RedirectEfficiency, bNeedsAirborneTransport ? TEXT("AIRBORNE") : TEXT("GROUND"));
}
