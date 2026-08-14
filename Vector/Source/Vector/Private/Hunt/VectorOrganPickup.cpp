// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hunt/VectorOrganPickup.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Hunt/VectorHuntProgressComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorOrganPickup, Log, All);

AVectorOrganPickup::AVectorOrganPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	SetRootComponent(PickupCollision);
	PickupCollision->InitSphereRadius(60.0f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollision->SetGenerateOverlapEvents(true);
	PickupCollision->OnComponentBeginOverlap.AddDynamic(
		this, &AVectorOrganPickup::HandleOverlap);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PickupCollision);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeLocation(FVector(0.0, 0.0, 42.0));
	PickupMesh->SetRelativeScale3D(FVector(0.42, 0.30, 0.55));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PickupMesh->SetStaticMesh(SphereMesh.Object);
	}

	PickupLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PickupLight"));
	PickupLight->SetupAttachment(PickupCollision);
	PickupLight->SetRelativeLocation(FVector(0.0, 0.0, 55.0));
	PickupLight->SetLightColor(FLinearColor(0.2f, 1.0f, 0.35f));
	PickupLight->SetIntensity(850.0f);
	PickupLight->SetAttenuationRadius(220.0f);

	RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
	RotatingMovement->RotationRate = FRotator(0.0, 110.0, 35.0);
}

void AVectorOrganPickup::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogVectorOrganPickup, Log,
		TEXT("Organ drop ready: pickup=%s amount=%d location=%s"),
		*GetName(), OrganAmount, *GetActorLocation().ToCompactString());
}

void AVectorOrganPickup::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AliveSeconds += FMath::Max(0.0f, DeltaSeconds);
	if (PickupMesh)
	{
		FVector RelativeLocation = PickupMesh->GetRelativeLocation();
		RelativeLocation.Z = 42.0 + FMath::Sin(AliveSeconds * 3.0) * 14.0;
		PickupMesh->SetRelativeLocation(RelativeLocation);
	}
}

void AVectorOrganPickup::HandleOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bCollected || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	AVectorGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AVectorGameMode>() : nullptr;
	UVectorHuntProgressComponent* HuntProgress = GameMode
		? GameMode->HuntProgress : nullptr;
	if (!HuntProgress)
	{
		UE_LOG(LogVectorOrganPickup, Warning,
			TEXT("Organ pickup rejected: no hunt ledger pickup=%s player=%s"),
			*GetName(), *GetNameSafe(OtherActor));
		return;
	}

	bCollected = true;
	const int32 NewTotal = HuntProgress->CollectOrgans(FMath::Max(1, OrganAmount));
	UE_LOG(LogVectorOrganPickup, Log,
		TEXT("Organ pickup consumed: pickup=%s player=%s total=%d"),
		*GetName(), *GetNameSafe(OtherActor), NewTotal);
	Destroy();
}
