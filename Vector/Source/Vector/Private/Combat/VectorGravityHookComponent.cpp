// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorGravityHookComponent.h"

#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "Combat/VectorGravityHookMath.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorGravityHook, Log, All);

namespace
{
	const TCHAR* GetCableModeLabel(const EVectorGravityHookMode Mode)
	{
		switch (Mode)
		{
		case EVectorGravityHookMode::AwaitingSecondEndpoint: return TEXT("AwaitingSecondEndpoint");
		case EVectorGravityHookMode::PullingPlayerToAnchor: return TEXT("PullingPlayerToAnchor");
		case EVectorGravityHookMode::RetractingPair: return TEXT("RetractingPair");
		default: return TEXT("None");
		}
	}

	bool IsAliveTarget(const AActor* Target)
	{
		if (!IsValid(Target))
		{
			return false;
		}
		const UVectorHealthComponent* Health =
			Target->FindComponentByClass<UVectorHealthComponent>();
		return !Health || !Health->IsDead();
	}

	double GetPhysicalMass(const AActor* Target)
	{
		const UVectorStabilityComponent* Stability = Target
			? Target->FindComponentByClass<UVectorStabilityComponent>() : nullptr;
		return Stability ? Stability->GetEffectivePhysicalMass() : 2.5;
	}
}

UVectorGravityHookComponent::UVectorGravityHookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	Timeline.bActiveUntilReleased = true;
	Timeline.RecoverySeconds = 0.20;
}

void UVectorGravityHookComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CooldownSecondsRemaining = FMath::Max(
		0.0,
		CooldownSecondsRemaining - FMath::Max(0.0f, DeltaTime));
	const EVectorActionPhase PreviousPhase = Timeline.Phase;
	Timeline.Advance(DeltaTime);

	switch (HookMode)
	{
	case EVectorGravityHookMode::AwaitingSecondEndpoint:
		UpdateFirstEndpoint(DeltaTime);
		break;
	case EVectorGravityHookMode::PullingPlayerToAnchor:
		UpdateWallPull();
		break;
	case EVectorGravityHookMode::RetractingPair:
		UpdatePair(DeltaTime);
		break;
	default:
		break;
	}

	DrawCableDebug();
	if (PreviousPhase != EVectorActionPhase::Idle
		&& Timeline.Phase == EVectorActionPhase::Idle)
	{
		ReleaseActionLock();
	}
}

void UVectorGravityHookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelHook();
	Super::EndPlay(EndPlayReason);
}

void UVectorGravityHookComponent::StartHook()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 活动配对期间再次按下视为主动剪断；单绳限制保持清楚且可自救。
	if (HookMode == EVectorGravityHookMode::RetractingPair)
	{
		BreakPair(TEXT("manual cancel"));
		return;
	}
	if (HookMode == EVectorGravityHookMode::PullingPlayerToAnchor)
	{
		return;
	}

	bool bUsedCursorAim = false;
	const FVector AimDirection = FVectorCombatTargeting::ComputeCursorGroundAimDirection(
		Owner, &bUsedCursorAim);
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable fire aim: source=%s direction=%s"),
		bUsedCursorAim ? TEXT("mouse") : TEXT("control-yaw fallback"),
		*AimDirection.ToCompactString());
	if (HookMode == EVectorGravityHookMode::AwaitingSecondEndpoint)
	{
		AActor* Target = FVectorCombatTargeting::FindMostAlignedMovableStableTarget(
			Owner, AimDirection, HookRangeCm, HookRadiusCm, FirstTarget.Get());
		if (!Target || !TryBeginPair(Target))
		{
			UE_LOG(LogVectorGravityHook, Log,
				TEXT("Cable second shot: NO valid second monster target"));
		}
		return;
	}

	if (Timeline.IsBusy() || CooldownSecondsRemaining > 0.0)
	{
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable start REJECTED: phase=%s cooldown=%.2f"),
			*UEnum::GetValueAsString(Timeline.Phase),
			CooldownSecondsRemaining);
		return;
	}

	AActor* Target = FVectorCombatTargeting::FindMostAlignedMovableStableTarget(
		Owner, AimDirection, HookRangeCm, HookRadiusCm);
	if (Target)
	{
		if (!TryAcquireActionLock() || !Timeline.TryStartActive())
		{
			ReleaseActionLock();
			return;
		}
		BeginFirstEndpoint(Target);
		return;
	}

	FVector AnchorPoint = FVector::ZeroVector;
	if (FindStaticAnchor(AimDirection, AnchorPoint))
	{
		if (!TryAcquireActionLock() || !Timeline.TryStartActive())
		{
			ReleaseActionLock();
			return;
		}
		BeginWallPull(AnchorPoint);
		return;
	}

	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable start: NO valid target in %.0fcm"), HookRangeCm);
	StartCooldown();
}

void UVectorGravityHookComponent::ReleaseHook()
{
	// 怪物端点采用两次点击；按钮松开不能取消第一端点或活动配对。
	if (HookMode == EVectorGravityHookMode::PullingPlayerToAnchor)
	{
		BeginWallReleaseMomentumCarry();
		FinishTransientAction(TEXT("input released"));
	}
}

void UVectorGravityHookComponent::HolsterHook()
{
	// Once two monsters are paired the cable is world state, not a held action.
	// Keeping it alive is what allows 2 -> place cable, 1 -> hammer the pair.
	if (HookMode == EVectorGravityHookMode::RetractingPair)
	{
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable pair preserved while equipment is holstered"));
		return;
	}
	if (HookMode == EVectorGravityHookMode::PullingPlayerToAnchor)
	{
		ReleaseHook();
		return;
	}
	if (HookMode == EVectorGravityHookMode::AwaitingSecondEndpoint)
	{
		FinishTransientAction(TEXT("equipment switched"));
	}
}

void UVectorGravityHookComponent::CancelHook()
{
	ClearPairImpactDelegates();
	FirstTarget.Reset();
	SecondTarget.Reset();
	WallAnchorPoint = FVector::ZeroVector;
	HookMode = EVectorGravityHookMode::None;
	SecondShotSecondsRemaining = 0.0;
	PairSecondsRemaining = 0.0;
	PairSetupSecondsRemaining = 0.0;
	PairSwingSecondsRemaining = 0.0;
	PairReelPauseSecondsRemaining = 0.0;
	PairCableLengthCm = 0.0;
	CooldownSecondsRemaining = 0.0;
	PairSpecificAngularMomentum = FVector::ZeroVector;
	DiagnosticLogSecondsRemaining = 0.0;
	bPairImpactSeen = false;
	Timeline.Reset();
	ReleaseActionLock();
}

bool UVectorGravityHookComponent::TryAcquireActionLock()
{
	if (bOwnsActionLock)
	{
		return true;
	}
	UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr;
	if (!Lock)
	{
		return true;
	}
	if (!Lock->TryAcquire(this, TEXT("CableGun")))
	{
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable start REJECTED: action lock held by %s"),
			*Lock->GetActiveActionName().ToString());
		return false;
	}
	bOwnsActionLock = true;
	return true;
}

void UVectorGravityHookComponent::ReleaseActionLock()
{
	if (!bOwnsActionLock)
	{
		return;
	}
	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		Lock->Release(this);
	}
	bOwnsActionLock = false;
}

bool UVectorGravityHookComponent::FindStaticAnchor(
	const FVector& AimDirection,
	FVector& OutAnchorPoint) const
{
	OutAnchorPoint = FVector::ZeroVector;
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || HookRangeCm <= 0.0)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VectorCableWallTargeting), false, Owner);
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + AimDirection.GetSafeNormal2D() * HookRangeCm;
	if (!World->SweepSingleByObjectType(
		Hit,
		Start,
		End,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic)),
		FCollisionShape::MakeSphere(FMath::Max(0.0, WallTargetingRadiusCm)),
		Params))
	{
		return false;
	}
	OutAnchorPoint = Hit.ImpactPoint;
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable wall hit: actor=%s anchor=%s sweepRadius=%.0f"),
		Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(world static)"),
		*OutAnchorPoint.ToCompactString(), WallTargetingRadiusCm);
	return FVectorGravityHookMath::IsFiniteVector(OutAnchorPoint);
}

void UVectorGravityHookComponent::StartCooldown()
{
	CooldownSecondsRemaining = FMath::Max(
		CooldownSecondsRemaining,
		FMath::Max(0.0, CableCooldownSeconds));
}

void UVectorGravityHookComponent::BeginFirstEndpoint(AActor* Target)
{
	FirstTarget = Target;
	SecondTarget.Reset();
	HookMode = EVectorGravityHookMode::AwaitingSecondEndpoint;
	SecondShotSecondsRemaining = FMath::Max(0.1, SecondShotWindowSeconds);
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable first endpoint: target=%s secondWindow=%.2fs"),
		Target ? *Target->GetName() : TEXT("(none)"),
		SecondShotSecondsRemaining);
}

bool UVectorGravityHookComponent::TryBeginPair(AActor* Target)
{
	AActor* First = FirstTarget.Get();
	if (!IsAliveTarget(First) || !IsAliveTarget(Target) || First == Target)
	{
		return false;
	}
	const double Distance = FVector::Dist(
		First->GetActorLocation(), Target->GetActorLocation());
	if (Distance <= UE_SMALL_NUMBER || Distance > MaximumPairDistanceCm
		|| !FVectorCombatTargeting::HasUnobstructedLine(First, Target))
	{
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable pair REJECTED: first=%s second=%s distance=%.0f max=%.0f line=%s"),
			*First->GetName(), *Target->GetName(), Distance, MaximumPairDistanceCm,
			FVectorCombatTargeting::HasUnobstructedLine(First, Target)
				? TEXT("clear") : TEXT("blocked"));
		return false;
	}

	UVectorCharacterMovementComponent* MovementA =
		First->FindComponentByClass<UVectorCharacterMovementComponent>();
	UVectorCharacterMovementComponent* MovementB =
		Target->FindComponentByClass<UVectorCharacterMovementComponent>();
	if (!MovementA || !MovementB)
	{
		return false;
	}

	SecondTarget = Target;
	HookMode = EVectorGravityHookMode::RetractingPair;
	PairSecondsRemaining = FMath::Max(0.1, MaximumPairLifetimeSeconds);
	PairSetupSecondsRemaining = FMath::Max(0.0, PairSetupGraceSeconds);
	PairSwingSecondsRemaining = 0.0;
	PairReelPauseSecondsRemaining = 0.0;
	PairCableLengthCm = Distance;
	bPairImpactSeen = false;
	PairSpecificAngularMomentum = FVectorGravityHookMath::ComputeSpatialSpecificAngularMomentum(
		First->GetActorLocation(),
		Target->GetActorLocation(),
		MovementA->GetEffectiveVelocityForPendingStep(),
		MovementB->GetEffectiveVelocityForPendingStep());
	DiagnosticLogSecondsRemaining = 0.0;
	BindPairImpactDelegates();

	// 配对完成后动作已经提交；活动绳是世界状态，不应继续占用玩家装备锁。
	Timeline.Reset();
	ReleaseActionLock();
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable pair attached: first=%s(m=%.2f) second=%s(m=%.2f) distance=%.0f cable=%.0f h=%.0f hammerWindow=%.2f lifetime=%.2f"),
		*First->GetName(), GetPhysicalMass(First),
		*Target->GetName(), GetPhysicalMass(Target),
		Distance, PairCableLengthCm, PairSpecificAngularMomentum.Size(),
		PairSetupSecondsRemaining, PairSecondsRemaining);
	return true;
}

void UVectorGravityHookComponent::BeginWallPull(const FVector& AnchorPoint)
{
	WallAnchorPoint = AnchorPoint;
	HookMode = EVectorGravityHookMode::PullingPlayerToAnchor;
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable wall attached: owner=%s anchor=%s distance=%.0f"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
		*WallAnchorPoint.ToCompactString(),
		GetOwner() ? FVector::Dist2D(GetOwner()->GetActorLocation(), WallAnchorPoint) : 0.0);
}

void UVectorGravityHookComponent::UpdateFirstEndpoint(const float DeltaTime)
{
	AActor* Owner = GetOwner();
	AActor* First = FirstTarget.Get();
	SecondShotSecondsRemaining -= FMath::Max(0.0f, DeltaTime);
	if (!Owner || !IsAliveTarget(First))
	{
		FinishTransientAction(TEXT("first endpoint lost"));
		return;
	}
	if (!FVectorCombatTargeting::HasUnobstructedLine(Owner, First))
	{
		FinishTransientAction(TEXT("first endpoint occluded"));
		return;
	}
	if (SecondShotSecondsRemaining <= 0.0)
	{
		FinishTransientAction(TEXT("second shot timeout"));
	}
}

void UVectorGravityHookComponent::UpdateWallPull()
{
	AActor* Owner = GetOwner();
	UVectorCharacterMovementComponent* Movement = Owner
		? Owner->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	if (!Owner || !Movement)
	{
		FinishTransientAction(TEXT("owner movement lost"));
		return;
	}

	const FVector ToAnchor = FVector::VectorPlaneProject(
		WallAnchorPoint - Owner->GetActorLocation(), FVector::UpVector);
	const double Distance = ToAnchor.Size();
	if (Distance <= FMath::Max(0.0, PlayerAnchorStopDistanceCm))
	{
		// 到达墙前安全距离时只消掉继续撞墙的径向分量；切向速度继续保留。
		const FVector CurrentVelocity = Movement->GetEffectiveVelocityForPendingStep();
		const FVector Direction = ToAnchor.GetSafeNormal();
		const double InwardSpeed = FVector::DotProduct(CurrentVelocity, Direction);
		const FVector SafeExitVelocity = InwardSpeed > 0.0
			? CurrentVelocity - Direction * InwardSpeed
			: CurrentVelocity;
		Movement->QueueWorldVelocityOverride(SafeExitVelocity);
		BeginWallReleaseMomentumCarry();
		FinishTransientAction(TEXT("anchor reached"));
		return;
	}

	const FVector Direction = ToAnchor / Distance;
	const FVector CurrentVelocity = Movement->GetEffectiveVelocityForPendingStep();
	FVector TangentialVelocity = FVector::VectorPlaneProject(
		CurrentVelocity - Direction * FVector::DotProduct(CurrentVelocity, Direction),
		FVector::UpVector);
	TangentialVelocity = TangentialVelocity.GetClampedToMaxSize(
		FMath::Max(0.0, MaximumPlayerTangentialSpeedCmPerSecond));
	FVector TargetVelocity = TangentialVelocity
		+ Direction * FMath::Max(0.0, PlayerReelSpeedCmPerSecond);
	TargetVelocity.Z = CurrentVelocity.Z;
	Movement->QueueWorldVelocityOverride(TargetVelocity);
	UE_LOG(LogVectorGravityHook, Verbose,
		TEXT("Cable wall reel: distance=%.0f radial=%.0f tangent=%.0f targetV=%s"),
		Distance, PlayerReelSpeedCmPerSecond, TangentialVelocity.Size(),
		*TargetVelocity.ToCompactString());
}

void UVectorGravityHookComponent::BeginWallReleaseMomentumCarry()
{
	UVectorCharacterMovementComponent* Movement = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	if (!Movement)
	{
		return;
	}
	Movement->BeginMomentumCarry(FMath::Max(0.0, WallReleaseMomentumCarrySeconds));
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable wall released with momentum: velocity=%s carry=%.2fs"),
		*Movement->GetEffectiveVelocityForPendingStep().ToCompactString(),
		WallReleaseMomentumCarrySeconds);
}

void UVectorGravityHookComponent::UpdatePair(const float DeltaTime)
{
	if (bPairImpactSeen)
	{
		BreakPair(TEXT("paired endpoints collided"));
		return;
	}

	AActor* First = FirstTarget.Get();
	AActor* Second = SecondTarget.Get();
	if (!IsAliveTarget(First) || !IsAliveTarget(Second))
	{
		BreakPair(TEXT("endpoint lost or dead"));
		return;
	}
	const double Distance = FVector::Dist(
		First->GetActorLocation(), Second->GetActorLocation());
	if (Distance <= UE_SMALL_NUMBER
		|| Distance > MaximumPairDistanceCm * 1.25
		|| !FVectorCombatTargeting::HasUnobstructedLine(First, Second))
	{
		BreakPair(TEXT("distance or static occlusion"));
		return;
	}
	const double SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (PairSetupSecondsRemaining > 0.0)
	{
		const double PreviousSetupSeconds = PairSetupSecondsRemaining;
		PairSetupSecondsRemaining = FMath::Max(
			0.0,
			PairSetupSecondsRemaining - SafeDeltaTime);
		if (PreviousSetupSeconds > 0.0 && PairSetupSecondsRemaining <= 0.0)
		{
			UE_LOG(LogVectorGravityHook, Log,
				TEXT("Cable pair hammer window ended; winch available after active swing"));
		}
	}
	else
	{
		PairSecondsRemaining -= SafeDeltaTime;
		if (PairSecondsRemaining <= 0.0)
		{
			BreakPair(TEXT("lifetime expired"));
			return;
		}
	}
	PairSwingSecondsRemaining = FMath::Max(
		0.0, PairSwingSecondsRemaining - SafeDeltaTime);
	if (PairReelPauseSecondsRemaining > 0.0)
	{
		PairReelPauseSecondsRemaining = FMath::Max(
			0.0,
			PairReelPauseSecondsRemaining - SafeDeltaTime);
		return;
	}

	UVectorCharacterMovementComponent* MovementA =
		First->FindComponentByClass<UVectorCharacterMovementComponent>();
	UVectorCharacterMovementComponent* MovementB =
		Second->FindComponentByClass<UVectorCharacterMovementComponent>();
	if (!MovementA || !MovementB)
	{
		BreakPair(TEXT("endpoint movement lost"));
		return;
	}

	const FVector VelocityA = MovementA->GetEffectiveVelocityForPendingStep();
	const FVector VelocityB = MovementB->GetEffectiveVelocityForPendingStep();
	const FVector CurrentAngularMomentum =
		FVectorGravityHookMath::ComputeSpatialSpecificAngularMomentum(
			First->GetActorLocation(), Second->GetActorLocation(), VelocityA, VelocityB);
	const bool bMeaningfulNewMomentum =
		CurrentAngularMomentum.Size() > PairSpecificAngularMomentum.Size()
			+ FMath::Max(0.0, AngularMomentumCaptureThreshold)
		|| (FVector::DotProduct(CurrentAngularMomentum, PairSpecificAngularMomentum) < 0.0
			&& CurrentAngularMomentum.Size() > FMath::Max(0.0, AngularMomentumCaptureThreshold));
	if (bMeaningfulNewMomentum)
	{
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable captured external angular momentum: %.0f -> %.0f; wide swing %.2fs"),
			PairSpecificAngularMomentum.Size(), CurrentAngularMomentum.Size(),
			HammerSwingDurationSeconds);
		PairSpecificAngularMomentum = CurrentAngularMomentum;
		PairSwingSecondsRemaining = FMath::Max(
			PairSwingSecondsRemaining,
			FMath::Max(0.0, HammerSwingDurationSeconds));
	}

	const bool bRopeExpectedTaut = Distance >= PairCableLengthCm
		- FMath::Max(0.0, RopeConstraintToleranceCm);
	if (!bRopeExpectedTaut)
	{
		// While slack, external movement is unconstrained and becomes the angular
		// momentum that will be preserved when the rope next becomes taut.
		PairSpecificAngularMomentum = CurrentAngularMomentum;
	}
	else
	{
		PairSpecificAngularMomentum *= FMath::Exp(
			-FMath::Max(0.0, AngularMomentumDampingPerSecond) * SafeDeltaTime);
	}

	const bool bWinchActive = PairSetupSecondsRemaining <= 0.0
		&& PairSwingSecondsRemaining <= 0.0;
	const double ActiveReelSpeed = bWinchActive
		? FMath::Max(0.0, PairReelSpeedCmPerSecond)
		: 0.0;
	PairCableLengthCm = FMath::Max(
		0.0, PairCableLengthCm - ActiveReelSpeed * SafeDeltaTime);

	FVector TargetVelocityA = FVector::ZeroVector;
	FVector TargetVelocityB = FVector::ZeroVector;
	bool bRopeTaut = false;
	const double MassA = GetPhysicalMass(First);
	const double MassB = GetPhysicalMass(Second);
	if (!FVectorGravityHookMath::SolveTetheredPairVelocities(
		First->GetActorLocation(),
		Second->GetActorLocation(),
		VelocityA,
		VelocityB,
		MassA,
		MassB,
		PairCableLengthCm,
		0.0,
		RopeConstraintToleranceCm,
		SafeDeltaTime,
		PairSpecificAngularMomentum,
		MaximumRelativeTangentialSpeedCmPerSecond,
		MaximumRopeCorrectionSpeedCmPerSecond,
		TargetVelocityA,
		TargetVelocityB,
		bRopeTaut))
	{
		BreakPair(TEXT("invalid pair solve"));
		return;
	}

	const FVector MomentumBefore = VelocityA * MassA + VelocityB * MassB;
	const FVector MomentumAfter = TargetVelocityA * MassA + TargetVelocityB * MassB;
	const bool bMomentumPass = MomentumBefore.Equals(
		MomentumAfter,
		FMath::Max(0.1, MomentumBefore.Size() * 1.e-5));
	MovementA->QueueWorldVelocityOverride(TargetVelocityA);
	MovementB->QueueWorldVelocityOverride(TargetVelocityB);
	DiagnosticLogSecondsRemaining -= FMath::Max(0.0f, DeltaTime);
	if (bLogCablePhysics && DiagnosticLogSecondsRemaining <= 0.0)
	{
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable pair solve: %s<->%s distance=%.0f cable=%.0f rope=%s mode=%s h=%.0f omega=%.2fdeg/s momentum=%s"),
			*First->GetName(), *Second->GetName(), Distance, PairCableLengthCm,
			bRopeTaut ? TEXT("TAUT") : TEXT("SLACK"),
			PairSwingSecondsRemaining > 0.0 ? TEXT("SWING")
				: (bWinchActive ? TEXT("WINCH") : TEXT("SETUP")),
			PairSpecificAngularMomentum.Size(),
			FMath::RadiansToDegrees(PairSpecificAngularMomentum.Size() / FMath::Square(Distance)),
			bMomentumPass ? TEXT("PASS") : TEXT("FAIL"));
		DiagnosticLogSecondsRemaining = FMath::Max(0.05, DiagnosticLogIntervalSeconds);
	}
	if (!bMomentumPass)
	{
		UE_LOG(LogVectorGravityHook, Error,
			TEXT("Cable pair momentum invariant failed: before=%s after=%s"),
			*MomentumBefore.ToCompactString(), *MomentumAfter.ToCompactString());
	}
}

void UVectorGravityHookComponent::FinishTransientAction(const TCHAR* Reason)
{
	const EVectorGravityHookMode FinishedMode = HookMode;
	const FString TargetName = FirstTarget.IsValid()
		? FirstTarget->GetName() : TEXT("(none)");
	FirstTarget.Reset();
	SecondTarget.Reset();
	WallAnchorPoint = FVector::ZeroVector;
	HookMode = EVectorGravityHookMode::None;
	SecondShotSecondsRemaining = 0.0;
	PairSwingSecondsRemaining = 0.0;
	PairCableLengthCm = 0.0;
	StartCooldown();
	if (!Timeline.TryEndActive())
	{
		Timeline.Reset();
		ReleaseActionLock();
	}
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable action ended: mode=%s target=%s reason=%s cooldown=%.2f"),
		GetCableModeLabel(FinishedMode), *TargetName,
		Reason ? Reason : TEXT("unknown"), CooldownSecondsRemaining);
}

void UVectorGravityHookComponent::BreakPair(const TCHAR* Reason)
{
	const FString FirstName = FirstTarget.IsValid()
		? FirstTarget->GetName() : TEXT("(lost)");
	const FString SecondName = SecondTarget.IsValid()
		? SecondTarget->GetName() : TEXT("(lost)");
	ClearPairImpactDelegates();
	FirstTarget.Reset();
	SecondTarget.Reset();
	HookMode = EVectorGravityHookMode::None;
	PairSecondsRemaining = 0.0;
	PairSetupSecondsRemaining = 0.0;
	PairSwingSecondsRemaining = 0.0;
	PairReelPauseSecondsRemaining = 0.0;
	PairCableLengthCm = 0.0;
	PairSpecificAngularMomentum = FVector::ZeroVector;
	DiagnosticLogSecondsRemaining = 0.0;
	bPairImpactSeen = false;
	StartCooldown();
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable pair broken: first=%s second=%s reason=%s cooldown=%.2f"),
		*FirstName, *SecondName,
		Reason ? Reason : TEXT("unknown"), CooldownSecondsRemaining);
}

void UVectorGravityHookComponent::BindPairImpactDelegates()
{
	ClearPairImpactDelegates();
	if (UVectorImpactCollisionComponent* ImpactA = FirstTarget.IsValid()
		? FirstTarget->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
	{
		FirstImpactDelegateHandle = ImpactA->OnBodyImpact.AddUObject(
			this, &UVectorGravityHookComponent::HandleEndpointBodyImpact);
		FirstSurfaceContactDelegateHandle = ImpactA->OnSurfaceContact.AddUObject(
			this, &UVectorGravityHookComponent::HandleEndpointSurfaceContact);
	}
	if (UVectorImpactCollisionComponent* ImpactB = SecondTarget.IsValid()
		? SecondTarget->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
	{
		SecondImpactDelegateHandle = ImpactB->OnBodyImpact.AddUObject(
			this, &UVectorGravityHookComponent::HandleEndpointBodyImpact);
		SecondSurfaceContactDelegateHandle = ImpactB->OnSurfaceContact.AddUObject(
			this, &UVectorGravityHookComponent::HandleEndpointSurfaceContact);
	}
}

void UVectorGravityHookComponent::ClearPairImpactDelegates()
{
	if (FirstImpactDelegateHandle.IsValid())
	{
		if (UVectorImpactCollisionComponent* ImpactA = FirstTarget.IsValid()
			? FirstTarget->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
		{
			ImpactA->OnBodyImpact.Remove(FirstImpactDelegateHandle);
		}
		FirstImpactDelegateHandle.Reset();
	}
	if (SecondImpactDelegateHandle.IsValid())
	{
		if (UVectorImpactCollisionComponent* ImpactB = SecondTarget.IsValid()
			? SecondTarget->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
		{
			ImpactB->OnBodyImpact.Remove(SecondImpactDelegateHandle);
		}
		SecondImpactDelegateHandle.Reset();
	}
	if (FirstSurfaceContactDelegateHandle.IsValid())
	{
		if (UVectorImpactCollisionComponent* ImpactA = FirstTarget.IsValid()
			? FirstTarget->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
		{
			ImpactA->OnSurfaceContact.Remove(FirstSurfaceContactDelegateHandle);
		}
		FirstSurfaceContactDelegateHandle.Reset();
	}
	if (SecondSurfaceContactDelegateHandle.IsValid())
	{
		if (UVectorImpactCollisionComponent* ImpactB = SecondTarget.IsValid()
			? SecondTarget->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
		{
			ImpactB->OnSurfaceContact.Remove(SecondSurfaceContactDelegateHandle);
		}
		SecondSurfaceContactDelegateHandle.Reset();
	}
}

void UVectorGravityHookComponent::HandleEndpointBodyImpact(AActor* OtherActor)
{
	if (HookMode != EVectorGravityHookMode::RetractingPair || bPairImpactSeen)
	{
		return;
	}
	const bool bPairedEndpointsCollided = OtherActor
		&& (OtherActor == FirstTarget.Get() || OtherActor == SecondTarget.Get());
	if (bPairedEndpointsCollided)
	{
		bPairImpactSeen = true;
		UE_LOG(LogVectorGravityHook, Log,
			TEXT("Cable pair endpoint collision: other=%s; break queued before next reel"),
			*OtherActor->GetName());
		return;
	}

	PairReelPauseSecondsRemaining = FMath::Max(
		PairReelPauseSecondsRemaining,
		FMath::Max(0.0, IncidentalBodyImpactPauseSeconds));
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable pair incidental impact: other=%s; cable kept, reelPause=%.2f"),
		OtherActor ? *OtherActor->GetName() : TEXT("(none)"),
		PairReelPauseSecondsRemaining);
}

void UVectorGravityHookComponent::HandleEndpointSurfaceContact(
	const double ImpactSpeedCmPerSecond)
{
	if (HookMode != EVectorGravityHookMode::RetractingPair || bPairImpactSeen)
	{
		return;
	}
	bPairImpactSeen = true;
	UE_LOG(LogVectorGravityHook, Log,
		TEXT("Cable pair surface contact observed: speed=%.0f; break queued before next reel"),
		ImpactSpeedCmPerSecond);
}

void UVectorGravityHookComponent::DrawCableDebug() const
{
	if (!bDrawHookDebug || !GetWorld())
	{
		return;
	}
	const FVector HeightOffset(0.0, 0.0, 60.0);
	if (HookMode == EVectorGravityHookMode::PullingPlayerToAnchor && GetOwner())
	{
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation() + HeightOffset,
			WallAnchorPoint + HeightOffset, FColor::Yellow, false, 0.03f, 0, 5.0f);
		DrawDebugSphere(GetWorld(), WallAnchorPoint + HeightOffset, 18.0f, 8,
			FColor::Yellow, false, 0.03f, 0, 2.0f);
	}
	else if (HookMode == EVectorGravityHookMode::AwaitingSecondEndpoint
		&& GetOwner() && FirstTarget.IsValid())
	{
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation() + HeightOffset,
			FirstTarget->GetActorLocation() + HeightOffset, FColor::Cyan, false, 0.03f, 0, 5.0f);
		DrawDebugSphere(GetWorld(), FirstTarget->GetActorLocation() + HeightOffset,
			30.0f, 12, FColor::Cyan, false, 0.03f, 0, 3.0f);
	}
	else if (HookMode == EVectorGravityHookMode::RetractingPair
		&& FirstTarget.IsValid() && SecondTarget.IsValid())
	{
		const FVector PositionA = FirstTarget->GetActorLocation();
		const FVector PositionB = SecondTarget->GetActorLocation();
		DrawDebugLine(GetWorld(), PositionA + HeightOffset, PositionB + HeightOffset,
			FColor::Magenta, false, 0.03f, 0, 7.0f);
		const FVector CenterOfMass = FVectorGravityHookMath::ComputeSpatialCenterOfMass(
			PositionA, PositionB, GetPhysicalMass(FirstTarget.Get()), GetPhysicalMass(SecondTarget.Get()));
		DrawDebugSphere(GetWorld(), CenterOfMass + HeightOffset, 25.0f, 12,
			FColor::Green, false, 0.03f, 0, 3.0f);
	}
}
