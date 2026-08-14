// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/VectorPhysicsModifierComponent.h"

#include "Combat/VectorTestDummy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Physics/VectorPhysicsModifierMath.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorModifier, Log, All);

UVectorPhysicsModifierComponent::UVectorPhysicsModifierComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVectorPhysicsModifierComponent::BeginPlay()
{
	Super::BeginPlay();
	CaptureBaseline();
}

void UVectorPhysicsModifierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LubricantSecondsRemaining = 0.0;
	BuoyantSecondsRemaining = 0.0;
	EnvironmentFrictionMultiplier = 1.0;
	ApplyEffectiveSettings();
	Super::EndPlay(EndPlayReason);
}

void UVectorPhysicsModifierComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	const bool bWasLubricated = IsLubricated();
	const bool bWasBuoyant = IsBuoyant();
	LubricantSecondsRemaining = FMath::Max(0.0, LubricantSecondsRemaining - DeltaTime);
	BuoyantSecondsRemaining = FMath::Max(0.0, BuoyantSecondsRemaining - DeltaTime);
	if (bWasLubricated != IsLubricated() || bWasBuoyant != IsBuoyant())
	{
		ApplyEffectiveSettings();
		UpdatePresentation();
		UE_LOG(LogVectorModifier, Log, TEXT("Modifier expired: actor=%s lubricant=%d buoyant=%d"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
			IsLubricated() ? 1 : 0,
			IsBuoyant() ? 1 : 0);
	}
	if (!IsLubricated() && !IsBuoyant())
	{
		SetComponentTickEnabled(false);
	}
}

void UVectorPhysicsModifierComponent::ApplyLubricant(const double DurationSeconds)
{
	CaptureBaseline();
	const double RequestedDuration = DurationSeconds >= 0.0 && FMath::IsFinite(DurationSeconds)
		? DurationSeconds
		: (FMath::IsFinite(LubricantDurationSeconds) ? LubricantDurationSeconds : 0.0);
	LubricantSecondsRemaining = FMath::Max(
		LubricantSecondsRemaining,
		FMath::Max(0.0, RequestedDuration));
	SetComponentTickEnabled(LubricantSecondsRemaining > 0.0 || IsBuoyant());
	ApplyEffectiveSettings();
	UpdatePresentation();
	UE_LOG(LogVectorModifier, Log, TEXT("Lubricant applied: actor=%s duration=%.1f frictionMultiplier=%.2f"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
		LubricantSecondsRemaining,
		LubricantFrictionMultiplier);
}

void UVectorPhysicsModifierComponent::ApplyBuoyantSpore(const double DurationSeconds)
{
	CaptureBaseline();
	const double RequestedDuration = DurationSeconds >= 0.0 && FMath::IsFinite(DurationSeconds)
		? DurationSeconds
		: (FMath::IsFinite(BuoyantDurationSeconds) ? BuoyantDurationSeconds : 0.0);
	BuoyantSecondsRemaining = FMath::Max(
		BuoyantSecondsRemaining,
		FMath::Max(0.0, RequestedDuration));
	SetComponentTickEnabled(BuoyantSecondsRemaining > 0.0 || IsLubricated());
	ApplyEffectiveSettings();
	UpdatePresentation();
	UE_LOG(LogVectorModifier, Log, TEXT("Buoyant spore applied: actor=%s duration=%.1f gravityMultiplier=%.2f"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
		BuoyantSecondsRemaining,
		BuoyantGravityMultiplier);
}

void UVectorPhysicsModifierComponent::SetEnvironmentFrictionMultiplier(const double Multiplier)
{
	CaptureBaseline();
	EnvironmentFrictionMultiplier = FMath::Clamp(
		FMath::IsFinite(Multiplier) ? Multiplier : 1.0,
		0.0,
		1.0);
	ApplyEffectiveSettings();
}

void UVectorPhysicsModifierComponent::ClearEnvironmentFrictionMultiplier()
{
	EnvironmentFrictionMultiplier = 1.0;
	ApplyEffectiveSettings();
}

void UVectorPhysicsModifierComponent::CaptureBaseline()
{
	if (bBaselineCaptured)
	{
		return;
	}
	UCharacterMovementComponent* CharacterMovement = GetOwner()
		? GetOwner()->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
	if (!CharacterMovement)
	{
		return;
	}
	Movement = CharacterMovement;
	BaseGroundFriction = CharacterMovement->GroundFriction;
	BaseBrakingFriction = CharacterMovement->BrakingFriction;
	BaseBrakingFrictionFactor = CharacterMovement->BrakingFrictionFactor;
	BaseBrakingDeceleration = CharacterMovement->BrakingDecelerationWalking;
	BaseGravityScale = CharacterMovement->GravityScale;
	bBaseUseSeparateBrakingFriction = CharacterMovement->bUseSeparateBrakingFriction;
	bBaselineCaptured = true;
}

void UVectorPhysicsModifierComponent::ApplyEffectiveSettings()
{
	CaptureBaseline();
	UCharacterMovementComponent* CharacterMovement = Movement.Get();
	if (!bBaselineCaptured || !CharacterMovement)
	{
		return;
	}

	const float EffectiveFrictionMultiplier = static_cast<float>(
		VectorPhysicsModifierMath::ComputeEffectiveFrictionMultiplier(
			EnvironmentFrictionMultiplier, IsLubricated(), LubricantFrictionMultiplier));
	const bool bFrictionModified = EffectiveFrictionMultiplier < 0.999f;
	if (bFrictionModified)
	{
		CharacterMovement->GroundFriction = BaseGroundFriction * EffectiveFrictionMultiplier;
		CharacterMovement->bUseSeparateBrakingFriction = true;
		CharacterMovement->BrakingFriction = FMath::Max(0.01f, BaseBrakingFriction * EffectiveFrictionMultiplier);
		CharacterMovement->BrakingFrictionFactor = 1.0f;
		CharacterMovement->BrakingDecelerationWalking =
			BaseBrakingDeceleration * FMath::Sqrt(EffectiveFrictionMultiplier);
	}
	else
	{
		CharacterMovement->GroundFriction = BaseGroundFriction;
		CharacterMovement->BrakingFriction = BaseBrakingFriction;
		CharacterMovement->BrakingFrictionFactor = BaseBrakingFrictionFactor;
		CharacterMovement->BrakingDecelerationWalking = BaseBrakingDeceleration;
		CharacterMovement->bUseSeparateBrakingFriction = bBaseUseSeparateBrakingFriction;
	}
	CharacterMovement->GravityScale = static_cast<float>(
		VectorPhysicsModifierMath::ComputeEffectiveGravityScale(
			BaseGravityScale, IsBuoyant(), BuoyantGravityMultiplier));
}

void UVectorPhysicsModifierComponent::UpdatePresentation() const
{
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(GetOwner()))
	{
		Dummy->SetPhysicsModifierPresentation(IsLubricated(), IsBuoyant());
	}
}
