// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorCircuitRoleMarker.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Non-interactive semantic marker for a real tactical-circuit node.
 * It never changes collision, navigation, damage, or encounter ledgers; its
 * only job is to make Source -> Converter -> Receiver readable in the room.
 */
UCLASS()
class VECTOR_API AVectorCircuitRoleMarker : public AActor
{
	GENERATED_BODY()

public:
	AVectorCircuitRoleMarker();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Vector|Circuit|Presentation")
	void ConfigureRole(FName NewRoleLabel, FLinearColor NewRoleColor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation")
	TObjectPtr<UStaticMeshComponent> GroundGlyph;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation")
	TObjectPtr<UTextRenderComponent> RoleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation")
	TObjectPtr<UPointLightComponent> RoleLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation")
	FName RoleLabel = TEXT("NODE");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation")
	FLinearColor RoleColor = FLinearColor(0.05f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Circuit|Presentation", meta = (ClampMin = "0.0"))
	double LightIntensity = 3600.0;

private:
	void RefreshPresentation();
};
