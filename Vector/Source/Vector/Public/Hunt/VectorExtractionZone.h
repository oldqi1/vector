// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorExtractionZone.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

/** Walk-through zone beyond the contract gate that finalizes the current hunt. */
UCLASS()
class VECTOR_API AVectorExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	AVectorExtractionZone();

protected:
	UFUNCTION()
	void HandleOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UPointLightComponent> MarkerLight;
};
