// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorGunComponent.h"

#include "Boss/VectorKineticOrb.h"
#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorBreakableAnchorComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "Combat/VectorEnemy.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "Combat/VectorKillAttributionComponent.h"
#include "Combat/VectorKillAttributionTypes.h"
#include "Combat/VectorTrajectoryPreviewComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Progression/VectorRunProgressionComponent.h"
#include "Stability/VectorStabilityComponent.h"
#include "VectorCharacter.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorGun, Log, All);

namespace
{
	UVectorKillAttributionComponent* FindVectorGunAttribution(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		const AGameModeBase* GameMode = World->GetAuthGameMode();
		return GameMode
			? GameMode->FindComponentByClass<UVectorKillAttributionComponent>() : nullptr;
	}
}

UVectorGunComponent::UVectorGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Timeline.ActiveSeconds = 0.05;
	Timeline.RecoverySeconds = 0.12;
	Timeline.MaxChargeSeconds = 0.0;
}

void UVectorGunComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentCells = GetMaximumCells();
}

void UVectorGunComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PreviewLogCooldownSecondsRemaining = FMath::Max(
		0.0, PreviewLogCooldownSecondsRemaining - FMath::Max(0.0f, DeltaTime));
	const EVectorActionPhase PreviousPhase = Timeline.Phase;
	Timeline.Advance(DeltaTime);
	if (PreviousPhase != EVectorActionPhase::Idle
		&& Timeline.Phase == EVectorActionPhase::Idle)
	{
		ReleaseActionLock();
	}
	UpdateAimPreview();

	const int32 MaximumCells = GetMaximumCells();
	CurrentCells = FMath::Clamp(CurrentCells, 0, MaximumCells);
	if (CurrentCells >= MaximumCells)
	{
		RechargeElapsedSeconds = 0.0;
		return;
	}
	const UVectorRunProgressionComponent* Progression = FindProgression();
	const double Interval = FMath::Max(
		0.05, BaseRechargeSecondsPerCell
			* (Progression ? Progression->GetRechargeIntervalMultiplier() : 1.0));
	RechargeElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	while (RechargeElapsedSeconds >= Interval && CurrentCells < MaximumCells)
	{
		RechargeElapsedSeconds -= Interval;
		++CurrentCells;
		UE_LOG(LogVectorGun, Verbose,
			TEXT("Vector gun cell recharged: owner=%s cells=%d/%d"),
			*GetNameSafe(GetOwner()), CurrentCells, MaximumCells);
	}
}

AActor* UVectorGunComponent::FindCursorTarget(
	const FVector& Direction,
	const double RangeCm) const
{
	return FVectorCombatTargeting::FindBestCursorMovableStableTarget(
		GetOwner(), Direction, RangeCm, TargetingRadiusCm,
		TargetingScreenRadiusPixels);
}

void UVectorGunComponent::UpdateAimPreview()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const AVectorCharacter* VectorCharacter = Cast<AVectorCharacter>(Owner);
	if (!Owner || !World || !VectorCharacter
		|| VectorCharacter->GetSelectedEquipmentSlot() != EVectorEquipmentSlot::Hammer)
	{
		PreviewTarget.Reset();
		return;
	}

	const FVector Direction =
		FVectorCombatTargeting::ComputeCursorGroundAimDirection(Owner).GetSafeNormal2D();
	AActor* Target = FindCursorTarget(Direction, GetEffectiveRangeCm());
	if (PreviewTarget.Get() != Target)
	{
		const UVectorCharacterMovementComponent* Movement = Target
			? Target->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
		const FVector EffectiveVelocity = Movement
			? Movement->GetEffectiveVelocityForPendingStep() : FVector::ZeroVector;
		const bool bAirborne = Movement
			&& (Movement->IsFalling()
				|| FMath::Abs(EffectiveVelocity.Z) > UE_KINDA_SMALL_NUMBER);
		if (PreviewLogCooldownSecondsRemaining <= 0.0)
		{
			UE_LOG(LogVectorGun, Log,
				TEXT("Vector preview target changed: previous=%s target=%s mode=%s selection=SCREEN_CURSOR check=PASS"),
				*GetNameSafe(PreviewTarget.Get()), *GetNameSafe(Target),
				Target ? (bAirborne ? TEXT("AIRBORNE") : TEXT("GROUND")) : TEXT("NONE"));
			PreviewLogCooldownSecondsRemaining = 0.25;
		}
		PreviewTarget = Target;
	}
	if (!Target)
	{
		return;
	}

	FVector BoundsOrigin = Target->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	const float MarkerRadius = static_cast<float>(FMath::Clamp(
		FMath::Max3(BoundsExtent.X, BoundsExtent.Y, BoundsExtent.Z * 0.5),
		45.0, 180.0));
	const FColor PreviewColor = CurrentCells > 0
		? FColor(30, 230, 255) : FColor(110, 120, 130);
	DrawDebugSphere(World, BoundsOrigin, MarkerRadius, 20,
		PreviewColor, false, 0.05f, 0, 3.0f);

	double Mass = 2.5;
	if (const UVectorStabilityComponent* Stability =
		Target->FindComponentByClass<UVectorStabilityComponent>())
	{
		Mass = Stability->GetEffectivePhysicalMass();
	}
	const UVectorCharacterMovementComponent* Movement =
		Target->FindComponentByClass<UVectorCharacterMovementComponent>();
	const FVector EffectiveVelocity = Movement
		? Movement->GetEffectiveVelocityForPendingStep() : Target->GetVelocity();
	const double InjectedSpeed = GetEffectiveImpulseBudget()
		/ FMath::Max(UE_SMALL_NUMBER, Mass);
	const double PreviewSpeed = FMath::Max(
		InjectedSpeed, EffectiveVelocity.Size2D() * 0.85);
	FVector PreviewVelocity = Direction * PreviewSpeed;
	if (Movement)
	{
		Movement->ComputeDirectionalVelocityOverride(
			Direction, PreviewSpeed, PreviewVelocity);
	}
	const float ArrowLength = static_cast<float>(FMath::Clamp(
		PreviewSpeed * 0.28, 100.0, 480.0));
	DrawDebugDirectionalArrow(
		World, BoundsOrigin,
		BoundsOrigin + PreviewVelocity.GetSafeNormal() * ArrowLength,
		55.0f, PreviewColor, false, 0.05f, 0, 5.0f);

	const bool bAirborne = Movement
		&& (Movement->IsFalling()
			|| FMath::Abs(EffectiveVelocity.Z) > UE_KINDA_SMALL_NUMBER);
	if (!bAirborne)
	{
		return;
	}
	if (UVectorTrajectoryPreviewComponent* TrajectoryPreview =
		Owner->FindComponentByClass<UVectorTrajectoryPreviewComponent>())
	{
		FVectorTrajectoryPreviewResult Prediction;
		TrajectoryPreview->PreviewBallisticPath(
			Target, Target->GetActorLocation(), PreviewVelocity,
			Movement ? Movement->GetGravityZ() : -980.0,
			PreviewColor, Prediction);
	}
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(
		SCENE_QUERY_STAT(VectorGunAirborneShadow), false, Target);
	GroundParams.AddIgnoredActor(Owner);
	if (World->LineTraceSingleByObjectType(
		GroundHit,
		BoundsOrigin,
		BoundsOrigin - FVector::UpVector * 5000.0,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic)),
		GroundParams))
	{
		const FVector ShadowPoint = GroundHit.ImpactPoint + FVector::UpVector * 3.0;
		DrawDebugLine(World, BoundsOrigin, ShadowPoint,
			PreviewColor, false, 0.05f, 0, 1.5f);
		DrawDebugCylinder(World,
			ShadowPoint - FVector::UpVector * 2.0,
			ShadowPoint + FVector::UpVector * 2.0,
			MarkerRadius * 0.75f, 24, PreviewColor,
			false, 0.05f, 0, 3.0f);
	}
}

void UVectorGunComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAction();
	Super::EndPlay(EndPlayReason);
}

bool UVectorGunComponent::Fire()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || CurrentCells <= 0 || Timeline.IsBusy())
	{
		UE_LOG(LogVectorGun, Log,
			TEXT("Vector gun rejected: owner=%s cells=%d busy=%d"),
			*GetNameSafe(Owner), CurrentCells, Timeline.IsBusy() ? 1 : 0);
		return false;
	}
	if (UVectorActionLockComponent* Lock =
		Owner->FindComponentByClass<UVectorActionLockComponent>())
	{
		if (!Lock->TryAcquire(this, TEXT("VectorGun")))
		{
			UE_LOG(LogVectorGun, Log,
				TEXT("Vector gun rejected: owner=%s reason=ACTION_LOCK"), *Owner->GetName());
			return false;
		}
		bOwnsActionLock = true;
	}
	if (!Timeline.TryStartActive())
	{
		ReleaseActionLock();
		return false;
	}

	--CurrentCells;
	RechargeElapsedSeconds = 0.0;
	const FVector Direction =
		FVectorCombatTargeting::ComputeCursorGroundAimDirection(Owner).GetSafeNormal2D();
	const double RangeCm = GetEffectiveRangeCm();
	AActor* Target = FindCursorTarget(Direction, RangeCm);
	PreviewTarget = Target;
	const FVector TraceEnd = Owner->GetActorLocation() + Direction * RangeCm;
	if (!Target)
	{
		DrawDebugLine(World, Owner->GetActorLocation(), TraceEnd,
			FColor::Red, false, 0.22f, 0, 4.0f);
		UE_LOG(LogVectorGun, Log,
			TEXT("Vector gun fired: owner=%s result=MISS range=%.0f cells=%d/%d direction=%s"),
			*Owner->GetName(), RangeCm, CurrentCells, GetMaximumCells(),
			*Direction.ToCompactString());
		return true;
	}
	if (AVectorKineticOrb* GuidedOrb = Cast<AVectorKineticOrb>(Target))
	{
		GuidedOrb->DisarmGuidance(TEXT("VECTOR_GUN"));
	}

	if (UVectorStabilityComponent* Stability =
		Target->FindComponentByClass<UVectorStabilityComponent>())
	{
		Stability->ReceiveImpactHit(
			BaseStabilityDamage, Stability->GetMassClass(), EVectorImpactType::Body);
	}

	double HealthDamage = BaseHealthDamage;
	double StructureDamageMultiplier = 1.0;
	if (const UVectorBreakableAnchorComponent* Anchors =
		Target->FindComponentByClass<UVectorBreakableAnchorComponent>();
		Anchors && Anchors->IsStructureEnabled())
	{
		if (Anchors->GetBrokenGroupCount() <= 0)
		{
			StructureDamageMultiplier = FMath::Clamp(
				AnchoredStructureHealthDamageMultiplier, 0.0, 1.0);
		}
		else if (!Anchors->IsLaunchable())
		{
			StructureDamageMultiplier = FMath::Clamp(
				UnstableStructureHealthDamageMultiplier, 0.0, 1.0);
		}
		HealthDamage *= StructureDamageMultiplier;
	}

	bool bKilled = false;
	if (UVectorHealthComponent* Health =
		Target->FindComponentByClass<UVectorHealthComponent>())
	{
		if (HealthDamage >= Health->GetHealth())
		{
			if (AVectorEnemy* Enemy = Cast<AVectorEnemy>(Target))
			{
				Enemy->PrepareForVectorGunLethalLaunch();
			}
		}
		bKilled = Health->ApplyDamage(HealthDamage);
		if (bKilled)
		{
			if (UVectorKillAttributionComponent* Attribution =
				FindVectorGunAttribution(World))
			{
				Attribution->RecordKill(EVectorKillCause::VectorGun);
			}
		}
	}

	double Mass = 2.5;
	FVector TargetVelocity = Target->GetVelocity();
	if (const UVectorStabilityComponent* Stability =
		Target->FindComponentByClass<UVectorStabilityComponent>())
	{
		Mass = Stability->GetEffectivePhysicalMass();
	}
	UVectorCharacterMovementComponent* TargetMovement =
		Target->FindComponentByClass<UVectorCharacterMovementComponent>();
	const bool bAirborneTarget = TargetMovement
		&& (TargetMovement->IsFalling()
			|| FMath::Abs(TargetVelocity.Z) > UE_KINDA_SMALL_NUMBER);
	UVectorImpactCollisionComponent* TargetImpact =
		Target->FindComponentByClass<UVectorImpactCollisionComponent>();
	const bool bLiftForkCombo = bAirborneTarget && TargetImpact
		&& TargetImpact->IsLiftForkVectorComboPrimed();
	const double InjectedSpeed = GetEffectiveImpulseBudget()
		/ FMath::Max(UE_SMALL_NUMBER, Mass)
		* (bLiftForkCombo ? FMath::Max(1.0, LiftForkComboImpulseMultiplier) : 1.0);
	const UVectorRunProgressionComponent* Progression = FindProgression();
	const bool bLiftVectorCoupler = bAirborneTarget && Progression
		&& Progression->HasRuleModule(EVectorRunModuleType::LiftVectorCoupler);
	const double PreservedSourceSpeed = bLiftVectorCoupler
		? TargetVelocity.Size() : TargetVelocity.Size2D();
	const double PreservedRedirectSpeed = PreservedSourceSpeed
		* (bLiftVectorCoupler ? 0.95 : 0.85);
	const double TargetSpeed = FMath::Max(InjectedSpeed, PreservedRedirectSpeed);
	bool bQueued = false;
	if (TargetMovement)
	{
		bQueued = TargetMovement->QueueDirectionalVelocityOverride(Direction, TargetSpeed);
	}
	if (bQueued && bLiftForkCombo)
	{
		TargetImpact->ConsumeLiftForkVectorCombo();
		TargetImpact->ArmNextLandingSource(FName(TEXT("LIFT_VECTOR_COMBO")));
	}
	if (bQueued && TargetMovement)
	{
		const FVector QueuedVelocity =
			TargetMovement->GetEffectiveVelocityForPendingStep();
		if (FMath::Abs(QueuedVelocity.Z) > UE_KINDA_SMALL_NUMBER)
		{
			if (UVectorTrajectoryPreviewComponent* TrajectoryPreview =
				Owner->FindComponentByClass<UVectorTrajectoryPreviewComponent>())
			{
				FVectorTrajectoryPreviewResult Prediction;
				if (TrajectoryPreview->PreviewBallisticPath(
					Target, Target->GetActorLocation(), QueuedVelocity,
					TargetMovement->GetGravityZ(), FColor::Cyan, Prediction))
				{
					TrajectoryPreview->ArmImpactVerification(Target, Prediction);
				}
			}
		}
	}

	if (Progression
		&& Progression->HasRuleModule(EVectorRunModuleType::MomentumRecycler)
		&& TargetVelocity.Size2D() >= MomentumRecyclerThresholdCmPerSecond)
	{
		CurrentCells = FMath::Min(GetMaximumCells(), CurrentCells + 1);
		UE_LOG(LogVectorGun, Log,
			TEXT("Vector module triggered: type=MOMENTUM_RECYCLER target=%s priorSpeed=%.0f refunded=1 cells=%d/%d check=PASS"),
			*Target->GetName(), TargetVelocity.Size2D(), CurrentCells, GetMaximumCells());
	}
	else if (Progression
		&& Progression->HasRuleModule(EVectorRunModuleType::TwinVector))
	{
		AActor* RelayTarget = FVectorCombatTargeting::FindMostAlignedMovableStableTarget(
			Owner, Direction, RangeCm, TargetingRadiusCm * 1.35, Target);
		if (RelayTarget)
		{
			double RelayMass = 2.5;
			if (const UVectorStabilityComponent* RelayStability =
				RelayTarget->FindComponentByClass<UVectorStabilityComponent>())
			{
				RelayMass = RelayStability->GetEffectivePhysicalMass();
			}
			const double RelaySpeed = GetEffectiveImpulseBudget()
				* FMath::Clamp(TwinVectorImpulseFraction, 0.0, 1.0)
				/ FMath::Max(UE_SMALL_NUMBER, RelayMass);
			const bool bRelayQueued = RelayTarget
				->FindComponentByClass<UVectorCharacterMovementComponent>()
				->QueueDirectionalVelocityOverride(Direction, RelaySpeed);
			DrawDebugLine(World, Target->GetActorLocation(), RelayTarget->GetActorLocation(),
				FColor::Purple, false, 0.35f, 0, 5.0f);
			UE_LOG(LogVectorGun, Log,
				TEXT("Vector module triggered: type=TWIN_VECTOR primary=%s relay=%s impulseFraction=%.2f mass=%.2f speed=%.0f queued=%s check=%s"),
				*Target->GetName(), *RelayTarget->GetName(), TwinVectorImpulseFraction,
				RelayMass, RelaySpeed, bRelayQueued ? TEXT("OK") : TEXT("REJECTED"),
				bRelayQueued ? TEXT("PASS") : TEXT("FAIL"));
		}
	}
	else if (Progression
		&& Progression->HasRuleModule(EVectorRunModuleType::LateralCutter))
	{
		if (UVectorBreakableAnchorComponent* Anchors =
			Target->FindComponentByClass<UVectorBreakableAnchorComponent>())
		{
			const FVectorAnchorStructureResult Result = Anchors->ApplyCollisionEvent(
				Direction, TargetSpeed, Owner);
			UE_LOG(LogVectorGun, Log,
				TEXT("Vector module triggered: type=LATERAL_CUTTER target=%s speed=%.0f broke=%s groups=%d/2 reason=%s check=%s"),
				*Target->GetName(), TargetSpeed,
				Result.bBrokeGroup ? TEXT("YES") : TEXT("no"), Result.BrokenGroupCount,
				*Result.Reason, Result.bAccepted ? TEXT("PASS") : TEXT("CONDITION_NOT_MET"));
		}
	}
	else if (bLiftVectorCoupler)
	{
		UE_LOG(LogVectorGun, Log,
			TEXT("Vector module triggered: type=LIFT_VECTOR_COUPLER target=%s airborne=YES prior3DSpeed=%.0f retainedSpeed=%.0f outputSpeed=%.0f energyGrant=%s check=PASS"),
			*Target->GetName(), TargetVelocity.Size(), PreservedRedirectSpeed,
			TargetSpeed,
			TargetSpeed > PreservedRedirectSpeed + UE_KINDA_SMALL_NUMBER
				? TEXT("VECTOR_CELL") : TEXT("NONE"));
	}
	if (bQueued && bLiftForkCombo)
	{
		UE_LOG(LogVectorGun, Log,
			TEXT("Vector combo triggered: type=LIFT_TO_VECTOR target=%s impulseMultiplier=%.2f baseInjectedSpeed=%.0f outputSpeed=%.0f landingProfile=FULL check=PASS"),
			*Target->GetName(), FMath::Max(1.0, LiftForkComboImpulseMultiplier),
			GetEffectiveImpulseBudget() / FMath::Max(UE_SMALL_NUMBER, Mass),
			TargetSpeed);
	}

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* OwnerMovement = OwnerCharacter->GetCharacterMovement())
		{
			OwnerMovement->AddImpulse(
				-Direction * FMath::Max(0.0, RecoilVelocityChangeCmPerSecond), true);
		}
	}
	DrawDebugLine(World, Owner->GetActorLocation(), Target->GetActorLocation(),
		FColor::Cyan, false, 0.28f, 0, 7.0f);
	DrawDebugDirectionalArrow(World, Target->GetActorLocation(),
		Target->GetActorLocation() + Direction * 240.0,
		70.0f, FColor::Cyan, false, 0.45f, 0, 8.0f);
	UE_LOG(LogVectorGun, Log,
		TEXT("Vector gun hit: owner=%s target=%s range=%.0f impulse=%.0f mass=%.2f speed=%.0f previousSpeed=%.0f previous3DSpeed=%.0f liftCombo=%s coupler=%s damage=%.1f structureMultiplier=%.2f killed=%s queued=%s cells=%d/%d direction=%s"),
		*Owner->GetName(), *Target->GetName(), RangeCm,
		GetEffectiveImpulseBudget(), Mass, TargetSpeed, TargetVelocity.Size2D(),
		TargetVelocity.Size(), bLiftForkCombo ? TEXT("ACTIVE") : TEXT("off"),
		bLiftVectorCoupler ? TEXT("ACTIVE") : TEXT("off"),
		HealthDamage, StructureDamageMultiplier, bKilled ? TEXT("YES") : TEXT("no"),
		bQueued ? TEXT("OK") : TEXT("REJECTED"), CurrentCells, GetMaximumCells(),
		*Direction.ToCompactString());
	return true;
}

void UVectorGunComponent::CancelAction()
{
	Timeline.Reset();
	ReleaseActionLock();
}

int32 UVectorGunComponent::GetMaximumCells() const
{
	const UVectorRunProgressionComponent* Progression = FindProgression();
	return FMath::Max(1, BaseMaximumCells
		+ (Progression ? Progression->GetAdditionalCells() : 0));
}

double UVectorGunComponent::GetEffectiveRangeCm() const
{
	const UVectorRunProgressionComponent* Progression = FindProgression();
	return FMath::Max(0.0, BaseRangeCm
		* (Progression ? Progression->GetRangeMultiplier() : 1.0));
}

double UVectorGunComponent::GetEffectiveImpulseBudget() const
{
	const UVectorRunProgressionComponent* Progression = FindProgression();
	return FMath::Max(0.0, BaseImpulseBudget
		* (Progression ? Progression->GetImpulseMultiplier() : 1.0));
}

double UVectorGunComponent::GetRechargeProgress() const
{
	if (CurrentCells >= GetMaximumCells())
	{
		return 1.0;
	}
	const UVectorRunProgressionComponent* Progression = FindProgression();
	const double Interval = FMath::Max(
		0.05, BaseRechargeSecondsPerCell
			* (Progression ? Progression->GetRechargeIntervalMultiplier() : 1.0));
	return FMath::Clamp(RechargeElapsedSeconds / Interval, 0.0, 1.0);
}

UVectorRunProgressionComponent* UVectorGunComponent::FindProgression() const
{
	return GetOwner()
		? GetOwner()->FindComponentByClass<UVectorRunProgressionComponent>() : nullptr;
}

void UVectorGunComponent::ReleaseActionLock()
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
