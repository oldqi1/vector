// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hunt/VectorContractExit.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Hunt/VectorEncounterComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorContractExit, Log, All);

AVectorContractExit::AVectorContractExit()
{
	PrimaryActorTick.bCanEverTick = false;

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	SetRootComponent(Blocker);
	Blocker->SetBoxExtent(FVector(45.0, 240.0, 180.0));
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(Blocker);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GateMesh->SetRelativeScale3D(FVector(0.9, 4.8, 3.6));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GateMesh->SetStaticMesh(CubeMesh.Object);
	}

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(Blocker);
	StatusLight->SetRelativeLocation(FVector(-75.0, 0.0, 180.0));
	StatusLight->SetLightColor(FLinearColor(1.0f, 0.05f, 0.02f));
	StatusLight->SetIntensity(4200.0f);
	StatusLight->SetAttenuationRadius(520.0f);
}

void AVectorContractExit::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AVectorContractExit::BindToEncounter));
	}
}

void AVectorContractExit::BindToEncounter()
{
	AVectorGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AVectorGameMode>() : nullptr;
	UVectorEncounterComponent* Encounter = GameMode
		? GameMode->Encounter : nullptr;
	if (!Encounter)
	{
		UE_LOG(LogVectorContractExit, Warning,
			TEXT("Contract exit has no encounter ledger: exit=%s"), *GetName());
		return;
	}

	Encounter->OnEncounterCompleted.AddUniqueDynamic(this, &AVectorContractExit::UnlockExit);
	Encounter->OnEncounterProgress.AddUniqueDynamic(
		this, &AVectorContractExit::HandleEncounterProgress);
	if (Encounter->IsComplete())
	{
		UnlockExit();
	}
	else if (Encounter->GetEncounterState() == EVectorEncounterState::Active)
	{
		HandleEncounterProgress(
			Encounter->GetRemainingEnemies(), Encounter->GetTotalEnemies());
	}
	else
	{
		UE_LOG(LogVectorContractExit, Verbose,
			TEXT("Contract exit waiting for encounter registration: exit=%s"), *GetName());
	}
}

void AVectorContractExit::HandleEncounterProgress(
	const int32 RemainingEnemies,
	const int32 TotalEnemies)
{
	if (bUnlocked || bLockedStateLogged || TotalEnemies <= 0)
	{
		return;
	}
	bLockedStateLogged = true;
	UE_LOG(LogVectorContractExit, Log,
		TEXT("Contract exit locked: exit=%s remaining=%d total=%d"),
		*GetName(), RemainingEnemies, TotalEnemies);
}

void AVectorContractExit::UnlockExit()
{
	if (bUnlocked)
	{
		return;
	}
	bUnlocked = true;
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GateMesh->SetVisibility(false, true);
	StatusLight->SetLightColor(FLinearColor(0.05f, 1.0f, 0.15f));
	StatusLight->SetIntensity(6500.0f);
	UE_LOG(LogVectorContractExit, Log,
		TEXT("Contract exit unlocked: exit=%s collision=OFF"), *GetName());
}
