// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCG/VectorPCGRoomTrigger.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "VectorCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorPCGRoomTrigger, Log, All);

AVectorPCGRoomTrigger::AVectorPCGRoomTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	TriggerBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBounds"));
	SetRootComponent(TriggerBounds);
	TriggerBounds->SetBoxExtent(FVector(180.0, 280.0, 180.0));
	TriggerBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBounds->SetGenerateOverlapEvents(true);
	TriggerBounds->SetCanEverAffectNavigation(false);
	TriggerBounds->OnComponentBeginOverlap.AddDynamic(
		this, &AVectorPCGRoomTrigger::HandleBeginOverlap);
}

void AVectorPCGRoomTrigger::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bConsumed || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	bConsumed = true;
	TriggerBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bUpdateRespawnCheckpoint)
	{
		if (AVectorCharacter* Character = Cast<AVectorCharacter>(OtherActor))
		{
			FTransform CheckpointTransform = GetActorTransform();
			CheckpointTransform.SetLocation(
				GetActorTransform().TransformPosition(CheckpointLocalOffset));
			CheckpointTransform.SetScale3D(FVector::OneVector);
			Character->SetRespawnCheckpoint(CheckpointTransform);
		}
	}
	UE_LOG(LogVectorPCGRoomTrigger, Log,
		TEXT("PCG room entered: trigger=%s room=%d player=%s"),
		*GetName(), RoomIndex, *Pawn->GetName());
	OnRoomEntered.Broadcast(RoomIndex);
}
