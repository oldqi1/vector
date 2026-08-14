// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorPCGRoomTrigger.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FVectorPCGRoomEnteredSignature,
	int32, RoomIndex);

/** One-shot player-only trigger that requests activation of a PCG room. */
UCLASS()
class VECTOR_API AVectorPCGRoomTrigger : public AActor
{
	GENERATED_BODY()

public:
	AVectorPCGRoomTrigger();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vector|PCG|Encounter")
	int32 RoomIndex = 0;

	UPROPERTY(BlueprintAssignable, Category = "Vector|PCG|Encounter")
	FVectorPCGRoomEnteredSignature OnRoomEntered;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PCG|Checkpoint")
	bool bUpdateRespawnCheckpoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PCG|Checkpoint")
	FVector CheckpointLocalOffset = FVector(-260.0, 0.0, -40.0);

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Vector|PCG|Encounter")
	TObjectPtr<UBoxComponent> TriggerBounds;

	bool bConsumed = false;
};
