// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/VectorLowFrictionZone.h"

#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Physics/VectorPhysicsModifierComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEnvironment, Log, All);

AVectorLowFrictionZone::AVectorLowFrictionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	SetRootComponent(ZoneBounds);
	ZoneBounds->SetBoxExtent(FVector(600.0, 600.0, 180.0));
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneBounds->SetCollisionObjectType(ECC_WorldDynamic);
	ZoneBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneBounds->SetGenerateOverlapEvents(true);
	ZoneBounds->SetCanEverAffectNavigation(false);
}

void AVectorLowFrictionZone::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bZoneActive && bDrawDebugBounds && GetWorld() && ZoneBounds)
	{
		DrawDebugBox(
			GetWorld(),
			ZoneBounds->GetComponentLocation(),
			ZoneBounds->GetScaledBoxExtent(),
			ZoneBounds->GetComponentQuat(),
			FColor::Cyan,
			false,
			0.03f,
			0,
			4.0f);
	}
}

void AVectorLowFrictionZone::BeginPlay()
{
	Super::BeginPlay();
	ZoneBounds->OnComponentBeginOverlap.AddDynamic(this, &AVectorLowFrictionZone::HandleBeginOverlap);
	ZoneBounds->OnComponentEndOverlap.AddDynamic(this, &AVectorLowFrictionZone::HandleEndOverlap);
	if (!bStartActive)
	{
		SetZoneActive(false);
		return;
	}

	// 兼容 Play 开始时已经位于区域内的角色。
	TArray<AActor*> InitiallyOverlappingActors;
	ZoneBounds->GetOverlappingActors(InitiallyOverlappingActors);
	for (AActor* Actor : InitiallyOverlappingActors)
	{
		if (Actor)
		{
			ApplyLowFriction(Actor->FindComponentByClass<UCharacterMovementComponent>());
		}
	}
}

void AVectorLowFrictionZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreAllAffectedMovement();
	Super::EndPlay(EndPlayReason);
}

void AVectorLowFrictionZone::SetZoneActive(const bool bActive)
{
	if (bZoneActive == bActive || !ZoneBounds)
	{
		return;
	}
	bZoneActive = bActive;
	SetActorTickEnabled(bZoneActive);
	ZoneBounds->SetCollisionEnabled(
		bZoneActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	ZoneBounds->SetGenerateOverlapEvents(bZoneActive);
	if (!bZoneActive)
	{
		RestoreAllAffectedMovement();
	}
	else
	{
		TArray<AActor*> OverlappingActors;
		ZoneBounds->GetOverlappingActors(OverlappingActors);
		for (AActor* Actor : OverlappingActors)
		{
			if (Actor)
			{
				ApplyLowFriction(Actor->FindComponentByClass<UCharacterMovementComponent>());
			}
		}
	}
	UE_LOG(LogVectorEnvironment, Log,
		TEXT("Low friction zone state: actor=%s active=%s affected=%d"),
		*GetName(), bZoneActive ? TEXT("YES") : TEXT("no"),
		OriginalSettings.Num() + ActiveModifierComponents.Num());
}

void AVectorLowFrictionZone::RestoreAllAffectedMovement()
{
	for (const TWeakObjectPtr<UVectorPhysicsModifierComponent>& Modifier : ActiveModifierComponents)
	{
		if (Modifier.IsValid())
		{
			Modifier->ClearEnvironmentFrictionMultiplier();
		}
	}
	ActiveModifierComponents.Empty();

	TArray<TWeakObjectPtr<UCharacterMovementComponent>> Movements;
	OriginalSettings.GetKeys(Movements);
	for (const TWeakObjectPtr<UCharacterMovementComponent>& Movement : Movements)
	{
		if (Movement.IsValid())
		{
			RestoreMovement(Movement.Get());
		}
	}
	OriginalSettings.Empty();
}

void AVectorLowFrictionZone::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bZoneActive && OtherActor)
	{
		ApplyLowFriction(OtherActor->FindComponentByClass<UCharacterMovementComponent>());
	}
}

void AVectorLowFrictionZone::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	// A character may expose more than one Pawn collision component. One component
	// ending its pair must not restore friction while another component is still
	// inside the same zone.
	if (ZoneBounds && ZoneBounds->IsOverlappingActor(OtherActor))
	{
		return;
	}
	RestoreMovement(OtherActor->FindComponentByClass<UCharacterMovementComponent>());
}

void AVectorLowFrictionZone::ApplyLowFriction(UCharacterMovementComponent* Movement)
{
	if (!Movement)
	{
		return;
	}
	if (UVectorPhysicsModifierComponent* Modifier = Movement->GetOwner()
		? Movement->GetOwner()->FindComponentByClass<UVectorPhysicsModifierComponent>() : nullptr)
	{
		const TWeakObjectPtr<UVectorPhysicsModifierComponent> ModifierKey(Modifier);
		if (!ActiveModifierComponents.Contains(ModifierKey))
		{
			ActiveModifierComponents.Add(ModifierKey);
			Modifier->SetEnvironmentFrictionMultiplier(ZoneFrictionMultiplier);
			UE_LOG(LogVectorEnvironment, Log,
				TEXT("Low friction ENTER: actor=%s unifiedMultiplier=%.3f"),
				*Movement->GetOwner()->GetName(), ZoneFrictionMultiplier);
		}
		return;
	}
	const TWeakObjectPtr<UCharacterMovementComponent> MovementKey(Movement);
	if (OriginalSettings.Contains(MovementKey))
	{
		return;
	}

	FMovementSettings& Saved = OriginalSettings.Add(MovementKey);
	Saved.GroundFriction = Movement->GroundFriction;
	Saved.BrakingFriction = Movement->BrakingFriction;
	Saved.BrakingFrictionFactor = Movement->BrakingFrictionFactor;
	Saved.BrakingDecelerationWalking = Movement->BrakingDecelerationWalking;
	Saved.bUseSeparateBrakingFriction = Movement->bUseSeparateBrakingFriction;

	Movement->GroundFriction = ZoneGroundFriction;
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = ZoneBrakingFriction;
	Movement->BrakingFrictionFactor = 1.0f;
	Movement->BrakingDecelerationWalking = ZoneBrakingDeceleration;

	UE_LOG(LogVectorEnvironment, Log,
		TEXT("Low friction ENTER: actor=%s ground=%.2f braking=%.2f decel=%.0f"),
		Movement->GetOwner() ? *Movement->GetOwner()->GetName() : TEXT("(none)"),
		Movement->GroundFriction,
		Movement->BrakingFriction,
		Movement->BrakingDecelerationWalking);
}

void AVectorLowFrictionZone::RestoreMovement(UCharacterMovementComponent* Movement)
{
	if (!Movement)
	{
		return;
	}
	if (UVectorPhysicsModifierComponent* Modifier = Movement->GetOwner()
		? Movement->GetOwner()->FindComponentByClass<UVectorPhysicsModifierComponent>() : nullptr)
	{
		const TWeakObjectPtr<UVectorPhysicsModifierComponent> ModifierKey(Modifier);
		if (ActiveModifierComponents.Remove(ModifierKey) > 0)
		{
			Modifier->ClearEnvironmentFrictionMultiplier();
			UE_LOG(LogVectorEnvironment, Log, TEXT("Low friction EXIT: actor=%s unified settings restored"),
				*Movement->GetOwner()->GetName());
		}
		return;
	}
	const TWeakObjectPtr<UCharacterMovementComponent> MovementKey(Movement);
	FMovementSettings* Saved = OriginalSettings.Find(MovementKey);
	if (!Saved)
	{
		return;
	}

	Movement->GroundFriction = Saved->GroundFriction;
	Movement->BrakingFriction = Saved->BrakingFriction;
	Movement->BrakingFrictionFactor = Saved->BrakingFrictionFactor;
	Movement->BrakingDecelerationWalking = Saved->BrakingDecelerationWalking;
	Movement->bUseSeparateBrakingFriction = Saved->bUseSeparateBrakingFriction;
	UE_LOG(LogVectorEnvironment, Log,
		TEXT("Low friction EXIT: actor=%s restored ground=%.2f braking=%.2f decel=%.0f"),
		Movement->GetOwner() ? *Movement->GetOwner()->GetName() : TEXT("(none)"),
		Movement->GroundFriction,
		Movement->BrakingFriction,
		Movement->BrakingDecelerationWalking);
	OriginalSettings.Remove(MovementKey);
}
