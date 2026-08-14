// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hunt/VectorExtractionZone.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/VectorKillAttributionComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Hunt/VectorEncounterComponent.h"
#include "Hunt/VectorHuntProgressComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorExtraction, Log, All);

AVectorExtractionZone::AVectorExtractionZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(180.0, 220.0, 140.0));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AVectorExtractionZone::HandleOverlap);

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(Trigger);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetRelativeLocation(FVector(0.0, 0.0, -135.0));
	MarkerMesh->SetRelativeScale3D(FVector(3.2, 3.2, 0.08));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		MarkerMesh->SetStaticMesh(CylinderMesh.Object);
	}

	MarkerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MarkerLight"));
	MarkerLight->SetupAttachment(Trigger);
	MarkerLight->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
	MarkerLight->SetLightColor(FLinearColor(0.05f, 1.0f, 0.35f));
	MarkerLight->SetIntensity(5000.0f);
	MarkerLight->SetAttenuationRadius(600.0f);
}

void AVectorExtractionZone::HandleOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	AVectorGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AVectorGameMode>() : nullptr;
	UVectorEncounterComponent* Encounter = GameMode ? GameMode->Encounter : nullptr;
	UVectorHuntProgressComponent* Hunt = GameMode ? GameMode->HuntProgress : nullptr;
	if (!Encounter || !Hunt)
	{
		UE_LOG(LogVectorExtraction, Warning,
			TEXT("Extraction rejected: missing game ledger player=%s"), *GetNameSafe(OtherActor));
		return;
	}
	if (!Encounter->IsComplete())
	{
		UE_LOG(LogVectorExtraction, Warning,
			TEXT("Extraction rejected: contract active remaining=%d player=%s"),
			Encounter->GetRemainingEnemies(), *GetNameSafe(OtherActor));
		return;
	}

	if (Hunt->CompleteExtraction())
	{
		const int32 TotalEnemies = Encounter->GetTotalEnemies();
		const double CollectionRate = TotalEnemies > 0
			? static_cast<double>(Hunt->GetSecuredOrgans()) / static_cast<double>(TotalEnemies)
			: 0.0;
		UE_LOG(LogVectorExtraction, Log,
			TEXT("Extraction completed: player=%s organs=%d"),
			*GetNameSafe(OtherActor), Hunt->GetSecuredOrgans());
		UE_LOG(LogVectorExtraction, Log,
			TEXT("Hunt run summary: elapsed=%.1fs defeated=%d/%d organs=%d collection=%.0f%%"),
			Encounter->GetElapsedSeconds(),
			TotalEnemies - Encounter->GetRemainingEnemies(), TotalEnemies,
			Hunt->GetSecuredOrgans(), CollectionRate * 100.0);
		if (GameMode->KillAttribution)
		{
			GameMode->KillAttribution->LogSummary();
		}
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MarkerLight->SetIntensity(8500.0f);
	}
}
