// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorOrganPickup.generated.h"

class UPointLightComponent;
class UPrimitiveComponent;
class URotatingMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Visible greybox organ dropped by an enemy and collected by the player. */
UCLASS()
class VECTOR_API AVectorOrganPickup : public AActor
{
	GENERATED_BODY()

public:
	AVectorOrganPickup();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Hunt", meta = (ClampMin = "1"))
	int32 OrganAmount = 1;

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
	TObjectPtr<USphereComponent> PickupCollision;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<UPointLightComponent> PickupLight;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Hunt")
	TObjectPtr<URotatingMovementComponent> RotatingMovement;

	double AliveSeconds = 0.0;
	bool bCollected = false;
};
