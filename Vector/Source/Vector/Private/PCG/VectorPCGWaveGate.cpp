// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCG/VectorPCGWaveGate.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorPCGWaveGate, Log, All);

AVectorPCGWaveGate::AVectorPCGWaveGate()
{
	PrimaryActorTick.bCanEverTick = false;
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	SetRootComponent(Blocker);
	// The adjacent tactical floors are much wider than the route spine. Seal the
	// whole transition so the player cannot walk around the progression gate.
	Blocker->SetBoxExtent(FVector(45.0, 1550.0, 700.0));
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// The gate is a runtime progression lock, not permanent level geometry.
	// Keeping it in the Recast bake leaves an invisible full-width nav cut after
	// collision is disabled; impulse-driven chargers cross while MoveTo agents stop.
	Blocker->SetCanEverAffectNavigation(false);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(Blocker);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GateMesh->SetRelativeScale3D(FVector(0.9, 31.0, 14.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GateMesh->SetStaticMesh(CubeMesh.Object);
	}

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(Blocker);
	StatusLight->SetRelativeLocation(FVector(-70.0, 0.0, 180.0));
	StatusLight->SetLightColor(FLinearColor(1.0f, 0.05f, 0.02f));
	StatusLight->SetIntensity(4200.0f);
	StatusLight->SetAttenuationRadius(520.0f);
}

void AVectorPCGWaveGate::SetGateLocked(const bool bLocked)
{
	if (bGateLocked == bLocked)
	{
		return;
	}
	bGateLocked = bLocked;
	Blocker->SetCollisionEnabled(
		bGateLocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	GateMesh->SetVisibility(bGateLocked, true);
	StatusLight->SetLightColor(
		bGateLocked ? FLinearColor(1.0f, 0.05f, 0.02f) : FLinearColor(0.05f, 1.0f, 0.15f));
	StatusLight->SetIntensity(bGateLocked ? 4200.0f : 6500.0f);
	UE_LOG(LogVectorPCGWaveGate, Log,
		TEXT("PCG wave gate: gate=%s locked=%s collision=%s navCut=NEVER"),
		*GetName(), bGateLocked ? TEXT("YES") : TEXT("no"),
		bGateLocked ? TEXT("ON") : TEXT("OFF"));
}
