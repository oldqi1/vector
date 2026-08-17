// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/VectorCircuitRoleMarker.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVectorCircuitRoleMarker::AVectorCircuitRoleMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetCanEverAffectNavigation(false);

	GroundGlyph = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundGlyph"));
	GroundGlyph->SetupAttachment(SceneRoot);
	GroundGlyph->SetRelativeLocation(FVector(0.0, 0.0, 4.0));
	GroundGlyph->SetRelativeScale3D(FVector(0.9, 0.9, 0.04));
	GroundGlyph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundGlyph->SetCanEverAffectNavigation(false);
	GroundGlyph->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		GroundGlyph->SetStaticMesh(CylinderMesh.Object);
	}

	RoleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RoleText"));
	RoleText->SetupAttachment(SceneRoot);
	RoleText->SetRelativeLocation(FVector(0.0, 0.0, 75.0));
	RoleText->SetHorizontalAlignment(EHTA_Center);
	RoleText->SetVerticalAlignment(EVRTA_TextCenter);
	RoleText->SetWorldSize(34.0f);
	RoleText->SetXScale(0.85f);
	RoleText->SetYScale(0.85f);
	RoleText->SetCanEverAffectNavigation(false);

	RoleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoleLight"));
	RoleLight->SetupAttachment(SceneRoot);
	RoleLight->SetRelativeLocation(FVector(0.0, 0.0, 65.0));
	RoleLight->SetAttenuationRadius(330.0f);
	RoleLight->SetCastShadows(false);

}

void AVectorCircuitRoleMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPresentation();
}

void AVectorCircuitRoleMarker::ConfigureRole(
	const FName NewRoleLabel,
	const FLinearColor NewRoleColor)
{
	RoleLabel = NewRoleLabel.IsNone() ? TEXT("NODE") : NewRoleLabel;
	RoleColor = NewRoleColor;
	RefreshPresentation();
}

void AVectorCircuitRoleMarker::RefreshPresentation()
{
	const FLinearColor SafeColor(
		FMath::Clamp(RoleColor.R, 0.0f, 1.0f),
		FMath::Clamp(RoleColor.G, 0.0f, 1.0f),
		FMath::Clamp(RoleColor.B, 0.0f, 1.0f),
		1.0f);
	if (RoleText)
	{
		RoleText->SetText(FText::FromName(RoleLabel));
		RoleText->SetTextRenderColor(SafeColor.ToFColor(true));
	}
	if (GroundGlyph)
	{
		if (!GlyphMaterial)
		{
			GlyphMaterial = GroundGlyph->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (GlyphMaterial)
		{
			GlyphMaterial->SetVectorParameterValue(TEXT("Color"), SafeColor);
		}
	}
	if (RoleLight)
	{
		RoleLight->SetLightColor(SafeColor);
		RoleLight->SetIntensity(static_cast<float>(FMath::Max(0.0, LightIntensity)));
	}
}
