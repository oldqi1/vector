// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorLiftForkComponent.h"

#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorCombatTargeting.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "Combat/VectorLiftForkMath.h"
#include "Combat/VectorTestDummy.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Impact/VectorImpactMath.h"
#include "Progression/VectorRunProgressionComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorLiftFork, Log, All);

namespace
{
	bool TraceSlamSurfaceAtXY(
		UWorld* World,
		const AActor* ToolOwner,
		const AActor* MovingTarget,
		const FVector& ReferencePoint,
		const double TraceHalfHeightCm,
		FHitResult& OutHit)
	{
		OutHit = FHitResult();
		if (!World || ReferencePoint.ContainsNaN()
			|| !FMath::IsFinite(TraceHalfHeightCm))
		{
			return false;
		}
		const double SafeHalfHeight = FMath::Max(500.0, TraceHalfHeightCm);
		const FVector TraceCenter(
			ReferencePoint.X, ReferencePoint.Y, ReferencePoint.Z);
		FCollisionQueryParams Params(
			SCENE_QUERY_STAT(VectorLiftForkSlamSurface), false, ToolOwner);
		Params.AddIgnoredActor(MovingTarget);
		return World->LineTraceSingleByObjectType(
			OutHit,
			TraceCenter + FVector::UpVector * SafeHalfHeight,
			TraceCenter - FVector::UpVector * SafeHalfHeight,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic)),
			Params)
			&& OutHit.bBlockingHit;
	}

	double ComputeSlamShapeSupport(
		const AActor* MovingTarget,
		const FVector& SurfaceNormal)
	{
		if (const ACharacter* Character = Cast<ACharacter>(MovingTarget))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				const double Radius = Capsule->GetScaledCapsuleRadius();
				const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
				return Radius + FMath::Max(0.0, HalfHeight - Radius)
					* FMath::Abs(SurfaceNormal.Z);
			}
		}
		return 42.0;
	}

	struct FDirectedSlamSolution
	{
		FVectorDirectedSlamResult Result;
		FVector RequestedSurfacePoint = FVector::ZeroVector;
		FVector ResolvedSurfacePoint = FVector::ZeroVector;
		FVector ResolvedSurfaceNormal = FVector::UpVector;
		FVector ResolvedTargetCenter = FVector::ZeroVector;
		double ResolvedAlpha = 0.0;
	};

	bool SolveDirectedSlam(
		UWorld* World,
		const AActor* Owner,
		const AActor* Target,
		const UVectorCharacterMovementComponent* Movement,
		const FVector& PreVelocity,
		const double MaximumSurfaceAngleDegrees,
		const double TraceHalfHeightCm,
		const double MinimumSpeedBudgetCmPerSecond,
		const FVector* RequestedSurfaceOverride,
		FDirectedSlamSolution& OutSolution)
	{
		OutSolution = FDirectedSlamSolution();
		FVector RequestedSurfaceNormal = FVector::UpVector;
		if (!World || !Owner || !Target || !Movement)
		{
			return false;
		}
		if (RequestedSurfaceOverride)
		{
			FHitResult OverrideHit;
			if (!TraceSlamSurfaceAtXY(
				World, Owner, Target, *RequestedSurfaceOverride,
				TraceHalfHeightCm, OverrideHit))
			{
				return false;
			}
			OutSolution.RequestedSurfacePoint = OverrideHit.ImpactPoint;
			RequestedSurfaceNormal = OverrideHit.ImpactNormal.GetSafeNormal();
		}
		else if (!FVectorCombatTargeting::ComputeCursorWorldStaticPoint(
			Owner, OutSolution.RequestedSurfacePoint, RequestedSurfaceNormal))
		{
			return false;
		}

		const FVector Start = Target->GetActorLocation();
		FVector StartGroundPoint(Start.X, Start.Y, OutSolution.RequestedSurfacePoint.Z);
		FHitResult StartGroundHit;
		if (TraceSlamSurfaceAtXY(
			World, Owner, Target, Start, TraceHalfHeightCm, StartGroundHit))
		{
			StartGroundPoint = StartGroundHit.ImpactPoint;
		}

		const double MinimumWalkableNormalZ = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(MaximumSurfaceAngleDegrees, 0.0, 89.0)));
		for (int32 AttemptIndex = 0; AttemptIndex <= 9; ++AttemptIndex)
		{
			const double Alpha = AttemptIndex == 0
				? 1.0 : 1.0 - AttemptIndex * 0.1;
			FVector CandidateSurfacePoint = OutSolution.RequestedSurfacePoint;
			FVector CandidateSurfaceNormal = RequestedSurfaceNormal;
			if (AttemptIndex > 0)
			{
				const FVector GroundGuess = FMath::Lerp(
					StartGroundPoint, OutSolution.RequestedSurfacePoint, Alpha);
				FHitResult CandidateGroundHit;
				if (!TraceSlamSurfaceAtXY(
					World, Owner, Target,
					FVector(GroundGuess.X, GroundGuess.Y,
						FMath::Max(Start.Z, OutSolution.RequestedSurfacePoint.Z)),
					TraceHalfHeightCm, CandidateGroundHit))
				{
					continue;
				}
				CandidateSurfacePoint = CandidateGroundHit.ImpactPoint;
				CandidateSurfaceNormal = CandidateGroundHit.ImpactNormal.GetSafeNormal();
			}
			if (CandidateSurfaceNormal.IsNearlyZero()
				|| CandidateSurfaceNormal.Z < MinimumWalkableNormalZ)
			{
				continue;
			}

			const double ShapeSupport = ComputeSlamShapeSupport(
				Target, CandidateSurfaceNormal);
			const FVector CandidateTargetCenter = CandidateSurfacePoint
				+ CandidateSurfaceNormal * ShapeSupport;
			const FVectorDirectedSlamResult CandidateResult =
				FVectorLiftForkMath::ComputeDirectedSlam(
					Start, CandidateTargetCenter, PreVelocity,
					Movement->GetGravityZ(), MinimumSpeedBudgetCmPerSecond);
			if (!CandidateResult.bValid
				|| !CandidateResult.IsWithinDeclaredBudget())
			{
				continue;
			}
			OutSolution.Result = CandidateResult;
			OutSolution.ResolvedSurfacePoint = CandidateSurfacePoint;
			OutSolution.ResolvedSurfaceNormal = CandidateSurfaceNormal;
			OutSolution.ResolvedTargetCenter = CandidateTargetCenter;
			OutSolution.ResolvedAlpha = Alpha;
			return true;
		}
		return false;
	}
}

UVectorLiftForkComponent::UVectorLiftForkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	Timeline.ActiveSeconds = 0.12;
	Timeline.RecoverySeconds = 0.45;
}

void UVectorLiftForkComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshAirborneCycleState();
	if (bForkGestureHeld)
	{
		ForkGestureHeldSeconds += FMath::Max(0.0f, DeltaTime);
		if (HasDirectedSlamUpgrade() && IsGestureHoldQualified())
		{
			if (IsGestureDragQualified())
			{
				DrawDirectedSlamPreview();
			}
			else
			{
				FVector AutomaticSurfacePoint = FVector::ZeroVector;
				int32 AutomaticClusterCount = 0;
				if (FindAutomaticSlamSurface(
					AutomaticSurfacePoint, AutomaticClusterCount))
				{
					DrawDirectedSlamPreview(
						&AutomaticSurfacePoint, AutomaticClusterCount);
				}
			}
		}
	}
	const EVectorActionPhase PreviousPhase = Timeline.Phase;
	Timeline.Advance(DeltaTime);
	if (PreviousPhase != EVectorActionPhase::Idle && Timeline.Phase == EVectorActionPhase::Idle)
	{
		ReleaseActionLock();
	}
}

void UVectorLiftForkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAction();
	Super::EndPlay(EndPlayReason);
}

void UVectorLiftForkComponent::BeginForkGesture()
{
	if (bForkGestureHeld)
	{
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift gesture REJECTED: reason=ALREADY_HELD"));
		return;
	}
	bForkGestureHeld = true;
	ForkGestureHeldSeconds = 0.0;
	bForkGestureHasCursor = false;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const APlayerController* PlayerController =
			Cast<APlayerController>(OwnerPawn->GetController()))
		{
			float CursorX = 0.0f;
			float CursorY = 0.0f;
			bForkGestureHasCursor = PlayerController->GetMousePosition(
				CursorX, CursorY);
			ForkGestureStartCursor = FVector2D(CursorX, CursorY);
		}
	}
	bool bUsedCursor = false;
	const FVector AimDirection = FVectorCombatTargeting::ComputeCursorGroundAimDirection(
		GetOwner(), &bUsedCursor);
	AActor* Target = AirborneFollowUpTarget.Get();
	UVectorCharacterMovementComponent* Movement = Target
		? Target->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	FVector PreVelocity = Movement
		? Movement->GetEffectiveVelocityForPendingStep() : FVector::ZeroVector;
	const bool bUsingLockedFollowUp = Target && Movement
		&& FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
			Movement->IsFalling(), PreVelocity);
	if (!bUsingLockedFollowUp)
	{
		AirborneFollowUpTarget.Reset();
		Target = FVectorCombatTargeting::FindMostAlignedMovableStableTarget(
			GetOwner(), AimDirection, ReachCm, RadiusCm);
		Movement = Target
			? Target->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
		PreVelocity = Movement
			? Movement->GetEffectiveVelocityForPendingStep() : FVector::ZeroVector;
	}
	if (!Target)
	{
		bForkGestureHeld = false;
		if (GetWorld() && GetOwner())
		{
			const FVector Start = GetOwner()->GetActorLocation();
			const FVector End = Start + AimDirection * ReachCm;
			DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.45f, 0, 6.0f);
			DrawDebugSphere(GetWorld(), End, static_cast<float>(RadiusCm), 20,
				FColor::Red, false, 0.45f, 0, 3.0f);
		}
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift fork: NO valid target aim=%s source=%s range=%.0f radius=%.0f"),
			*AimDirection.ToCompactString(), bUsedCursor ? TEXT("mouse") : TEXT("fallback"),
			ReachCm, RadiusCm);
		return;
	}
	UVectorStabilityComponent* Stability =
		Target->FindComponentByClass<UVectorStabilityComponent>();
	if (!Movement || !Stability)
	{
		bForkGestureHeld = false;
		return;
	}
	if (FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
		Movement->IsFalling(), PreVelocity))
	{
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift gesture armed: target=%s mode=AIRBORNE_FOLLOW_UP holdThenDrag=YES velocity=%s check=PASS"),
			*Target->GetName(), *PreVelocity.ToCompactString());
		return;
	}
	if (Timeline.IsBusy())
	{
		bForkGestureHeld = false;
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift gesture REJECTED: reason=OWN_ACTION_BUSY"));
		return;
	}

	if (UVectorActionLockComponent* Lock = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorActionLockComponent>() : nullptr)
	{
		if (!Lock->TryAcquire(this, TEXT("LiftFork")))
		{
			bForkGestureHeld = false;
			UE_LOG(LogVectorLiftFork, Log, TEXT("Lift fork REJECTED: action lock=%s"),
				*Lock->GetActiveActionName().ToString());
			return;
		}
		bOwnsActionLock = true;
	}
	if (!Timeline.TryStartActive())
	{
		bForkGestureHeld = false;
		ReleaseActionLock();
		return;
	}

	const FVectorLiftForkRedirectResult Redirect =
		FVectorLiftForkMath::ComputeVerticalRedirect(PreVelocity);
	if (!Redirect.bValid || !Redirect.IsWithinDeclaredBudget())
	{
		bForkGestureHeld = false;
		Timeline.Reset();
		ReleaseActionLock();
		UE_LOG(LogVectorLiftFork, Warning,
			TEXT("Lift fork REJECTED: invalid redirect target=%s preVelocity=%s valid=%d budgetCheck=%d"),
			*Target->GetName(), *PreVelocity.ToCompactString(), Redirect.bValid ? 1 : 0,
			Redirect.IsWithinDeclaredBudget() ? 1 : 0);
		return;
	}
	// Use CharacterMovement's canonical pending launch. It consumes velocity and
	// enters Falling in one engine step; the generic CalcVelocity override can be
	// flattened by a ground/air transition even when its queue log says success.
	// Submit the complete solver result. The directional helper intentionally
	// preserves perpendicular velocity, which would re-add speed outside H03's
	// declared budget and make the redirect depend on component decomposition.
	const bool bQueued = Movement->QueueAirborneWorldVelocityOverride(
		Redirect.OutputVelocity);
	if (!bQueued)
	{
		bForkGestureHeld = false;
		Timeline.Reset();
		ReleaseActionLock();
		UE_LOG(LogVectorLiftFork, Warning,
			TEXT("Lift fork REJECTED: velocity queue target=%s output=%s preSpeed=%.0f B=%.0f"),
			*Target->GetName(), *Redirect.OutputVelocity.ToCompactString(),
			Redirect.PreHorizontalSpeedCmPerSecond,
			Redirect.RedirectBudgetCmPerSecond);
		return;
	}
	AirborneFollowUpTarget = Target;
	if (UVectorImpactCollisionComponent* Impact =
		Target->FindComponentByClass<UVectorImpactCollisionComponent>())
	{
		Impact->PrimeLiftForkVectorCombo();
		Impact->ArmNextLandingSource(
			FName(TEXT("LIFT_NATURAL_FALL")),
			NaturalLandingMinimumFallSpeedCmPerSecond,
			NaturalLandingRadiusScale,
			NaturalLandingDamageScale,
			NaturalLandingMaximumDamage);
	}
	const double AppliedStabilityDamage = Stability->ReceiveImpactHit(
		StabilityDamage, Stability->GetMassClass(), EVectorImpactType::Ground);
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(Target))
	{
		Dummy->TriggerLiftForkPresentation();
	}
	if (GetWorld())
	{
		if (GetOwner())
		{
			DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), Target->GetActorLocation(),
				FColor::Yellow, false, 0.45f, 0, 8.0f);
		}
		DrawDebugDirectionalArrow(
			GetWorld(),
			Target->GetActorLocation(),
			Target->GetActorLocation()
				+ Redirect.OutputVelocity.GetSafeNormal() * 300.0,
			20.0f,
			FColor::Yellow,
			false,
			0.45f,
			0,
			8.0f);
	}
	UE_LOG(LogVectorLiftFork, Log,
		TEXT("Lift fork redirect: target=%s preVelocity=%s preSpeed=%.0f B=%.0f output=%s postVh=%.0f postVz=%.0f floorUsed=%s stabilityDamage=%.1f queued=OK followUp=LOCKED aimSource=%s currentMode=%s awaiting=PendingLaunch check=PASS"),
		*Target->GetName(), *PreVelocity.ToCompactString(),
		Redirect.PreHorizontalSpeedCmPerSecond,
		Redirect.RedirectBudgetCmPerSecond,
		*Redirect.OutputVelocity.ToCompactString(),
		Redirect.OutputVelocity.Size2D(), Redirect.OutputVelocity.Z,
		Redirect.bUsedVerticalFloor ? TEXT("YES") : TEXT("no"),
		AppliedStabilityDamage,
		bUsedCursor ? TEXT("mouse") : TEXT("fallback"),
		*Movement->GetMovementName());
}

void UVectorLiftForkComponent::ReleaseForkGesture()
{
	if (!bForkGestureHeld)
	{
		return;
	}
	const bool bSlamUnlocked = HasDirectedSlamUpgrade();
	const bool bHoldQualified = bSlamUnlocked && IsGestureHoldQualified();
	const bool bDragQualified = bHoldQualified && IsGestureDragQualified();
	bForkGestureHeld = false;

	AActor* Target = AirborneFollowUpTarget.Get();
	UVectorCharacterMovementComponent* Movement = Target
		? Target->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	const FVector PreVelocity = Movement
		? Movement->GetEffectiveVelocityForPendingStep() : FVector::ZeroVector;
	if (!bHoldQualified)
	{
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift gesture released: target=%s mode=LIFT_ONLY slamUpgrade=%s held=%.2f holdThreshold=%.2f check=PASS"),
			*GetNameSafe(Target), bSlamUnlocked ? TEXT("READY") : TEXT("LOCKED"),
			ForkGestureHeldSeconds, MinimumSlamHoldSeconds);
		return;
	}
	if (!Target || !Movement
		|| !FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
			Movement->IsFalling(), PreVelocity))
	{
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Lift gesture released: target=%s mode=SLAM_REJECTED reason=TARGET_LANDED_OR_INVALID velocityUnchanged=YES"),
			*GetNameSafe(Target));
		return;
	}

	FVector AutomaticSurfacePoint = FVector::ZeroVector;
	int32 AutomaticClusterCount = -1;
	const FVector* RequestedSurfaceOverride = nullptr;
	if (!bDragQualified)
	{
		AutomaticClusterCount = 0;
		if (!FindAutomaticSlamSurface(
			AutomaticSurfacePoint, AutomaticClusterCount))
		{
			UE_LOG(LogVectorLiftFork, Log,
				TEXT("Lift gesture released: target=%s mode=AUTO_SLAM_REJECTED reason=NO_VALID_DENSITY_SURFACE velocityUnchanged=YES"),
				*Target->GetName());
			return;
		}
		RequestedSurfaceOverride = &AutomaticSurfacePoint;
	}
	TryActivateDirectedSlam(
		Target, Movement, PreVelocity, RequestedSurfaceOverride,
		AutomaticClusterCount);
}

void UVectorLiftForkComponent::ActivateFork()
{
	BeginForkGesture();
	ReleaseForkGesture();
}

bool UVectorLiftForkComponent::IsGestureDragQualified() const
{
	if (!bForkGestureHasCursor)
	{
		return false;
	}
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	float CursorX = 0.0f;
	float CursorY = 0.0f;
	if (!PlayerController
		|| !PlayerController->GetMousePosition(CursorX, CursorY))
	{
		return false;
	}
	return FVectorLiftForkMath::IsSlamGestureQualified(
		ForkGestureHeldSeconds,
		FVector2D::Distance(
			ForkGestureStartCursor, FVector2D(CursorX, CursorY)),
		MinimumSlamHoldSeconds, MinimumSlamDragPixels);
}

bool UVectorLiftForkComponent::IsGestureHoldQualified() const
{
	return FMath::IsFinite(ForkGestureHeldSeconds)
		&& ForkGestureHeldSeconds >= FMath::Max(0.0, MinimumSlamHoldSeconds);
}

bool UVectorLiftForkComponent::HasDirectedSlamUpgrade() const
{
	if (bEnableDirectedSlamWithoutModule)
	{
		return true;
	}
	const UVectorRunProgressionComponent* Progression = GetOwner()
		? GetOwner()->FindComponentByClass<UVectorRunProgressionComponent>() : nullptr;
	return Progression
		&& Progression->HasRuleModule(EVectorRunModuleType::LiftVectorCoupler);
}

bool UVectorLiftForkComponent::FindAutomaticSlamSurface(
	FVector& OutSurfacePoint,
	int32& OutClusterCount) const
{
	OutSurfacePoint = FVector::ZeroVector;
	OutClusterCount = 0;
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	AActor* LiftedTarget = AirborneFollowUpTarget.Get();
	if (!World || !Owner || !LiftedTarget)
	{
		return false;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(VectorLiftForkAutoSlamDensity), false, LiftedTarget);
	Params.AddIgnoredActor(Owner);
	World->OverlapMultiByObjectType(
		Overlaps,
		LiftedTarget->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)),
		FCollisionShape::MakeSphere(FMath::Max(0.0, AutomaticSlamSearchRadiusCm)),
		Params);

	TArray<AActor*> Candidates;
	TSet<TWeakObjectPtr<AActor>> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const TWeakObjectPtr<AActor> CandidateKey(Candidate);
		if (!Candidate || Candidate == Owner || Candidate == LiftedTarget
			|| Seen.Contains(CandidateKey))
		{
			continue;
		}
		const UVectorHealthComponent* Health =
			Candidate->FindComponentByClass<UVectorHealthComponent>();
		if ((Health && Health->IsDead())
			|| (!Health
				&& !Candidate->FindComponentByClass<UVectorStabilityComponent>()))
		{
			continue;
		}
		Seen.Add(CandidateKey);
		Candidates.Add(Candidate);
	}

	FVector BestCentroid = LiftedTarget->GetActorLocation();
	FString BestStableName;
	int32 BestCount = 0;
	const double ClusterRadiusSquared = FMath::Square(
		FMath::Max(1.0, AutomaticSlamClusterRadiusCm));
	for (AActor* Seed : Candidates)
	{
		FVector ClusterSum = FVector::ZeroVector;
		int32 ClusterCount = 0;
		const FVector SeedLocation = Seed->GetActorLocation();
		for (AActor* Candidate : Candidates)
		{
			const FVector Delta = Candidate->GetActorLocation() - SeedLocation;
			if (FVector(Delta.X, Delta.Y, 0.0).SizeSquared()
				<= ClusterRadiusSquared)
			{
				ClusterSum += Candidate->GetActorLocation();
				++ClusterCount;
			}
		}
		const FString StableName = Seed->GetName();
		if (ClusterCount > BestCount
			|| (ClusterCount == BestCount
				&& (BestStableName.IsEmpty()
					|| StableName.Compare(BestStableName) < 0)))
		{
			BestCount = ClusterCount;
			BestStableName = StableName;
			BestCentroid = ClusterCount > 0
				? ClusterSum / static_cast<double>(ClusterCount) : SeedLocation;
		}
	}

	FHitResult GroundHit;
	if (!TraceSlamSurfaceAtXY(
		World, Owner, LiftedTarget, BestCentroid,
		SlamGroundTraceHalfHeightCm, GroundHit))
	{
		return false;
	}
	OutSurfacePoint = GroundHit.ImpactPoint;
	OutClusterCount = BestCount;
	return true;
}

void UVectorLiftForkComponent::DrawDirectedSlamPreview(
	const FVector* RequestedSurfaceOverride,
	const int32 AutomaticClusterCount)
{
	if (!HasDirectedSlamUpgrade())
	{
		return;
	}
	AActor* Target = AirborneFollowUpTarget.Get();
	const UVectorCharacterMovementComponent* Movement = Target
		? Target->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
	UWorld* World = GetWorld();
	if (!Target || !Movement || !World)
	{
		return;
	}
	const FVector PreVelocity = Movement->GetEffectiveVelocityForPendingStep();
	if (!FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
		Movement->IsFalling(), PreVelocity))
	{
		return;
	}

	FDirectedSlamSolution Solution;
	const bool bSolved = SolveDirectedSlam(
		World, GetOwner(), Target, Movement, PreVelocity,
		MaximumSlamSurfaceAngleDegrees, SlamGroundTraceHalfHeightCm,
		FVectorLiftForkMath::DirectedSlamUpgradeFloorCmPerSecond,
		RequestedSurfaceOverride,
		Solution);
	if (!bSolved)
	{
		if (!Solution.RequestedSurfacePoint.IsNearlyZero())
		{
			DrawDebugSphere(World, Solution.RequestedSurfacePoint, 48.0f, 16,
				FColor::Red, false, 0.0f, 0, 5.0f);
		}
		return;
	}

	DrawDebugSphere(World, Solution.RequestedSurfacePoint, 36.0f, 14,
		AutomaticClusterCount >= 0 ? FColor::Orange : FColor::Cyan,
		false, 0.0f, 0, 3.0f);
	DrawDebugSphere(
		World,
		Solution.ResolvedSurfacePoint + Solution.ResolvedSurfaceNormal * 4.0,
		52.0f, 18, FColor::Green, false, 0.0f, 0, 5.0f);
	DrawDebugDirectionalArrow(
		World, Target->GetActorLocation(),
		Target->GetActorLocation()
			+ Solution.Result.LaunchVelocity.GetSafeNormal() * 300.0,
		24.0f, FColor::Magenta, false, 0.0f, 0, 6.0f);
	FVector PreviousArcPoint = Target->GetActorLocation();
	constexpr int32 ArcSegments = 12;
	for (int32 SegmentIndex = 1; SegmentIndex <= ArcSegments; ++SegmentIndex)
	{
		const double TimeSeconds = Solution.Result.FlightSeconds
			* static_cast<double>(SegmentIndex) / ArcSegments;
		const FVector ArcPoint = FVectorImpactMath::SampleBallisticPosition(
			Target->GetActorLocation(), Solution.Result.LaunchVelocity,
			Movement->GetGravityZ(), TimeSeconds);
		DrawDebugLine(World, PreviousArcPoint, ArcPoint,
			FColor::Magenta, false, 0.0f, 0, 4.0f);
		PreviousArcPoint = ArcPoint;
	}
	if (Solution.ResolvedAlpha < 1.0)
	{
		DrawDebugLine(
			World, Solution.RequestedSurfacePoint, Solution.ResolvedSurfacePoint,
			FColor::Yellow, false, 0.0f, 0, 4.0f);
	}
}

bool UVectorLiftForkComponent::TryActivateDirectedSlam(
	AActor* Target,
	UVectorCharacterMovementComponent* Movement,
	const FVector& PreVelocity,
	const FVector* RequestedSurfaceOverride,
	const int32 AutomaticClusterCount)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (!World || !Owner || !Target || !Movement)
	{
		return false;
	}
	if (!HasDirectedSlamUpgrade())
	{
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Directed slam REJECTED: target=%s reason=UPGRADE_LOCKED velocityUnchanged=YES check=PASS"),
			*Target->GetName());
		return false;
	}
	if (SlammedTargetsThisAirborneCycle.Contains(TargetKey))
	{
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Directed slam REJECTED: target=%s reason=ALREADY_USED_THIS_AIRBORNE_CYCLE velocity=%s floorGrant=BLOCKED check=PASS"),
			*Target->GetName(), *PreVelocity.ToCompactString());
		return false;
	}

	FDirectedSlamSolution Solution;
	if (!SolveDirectedSlam(
		World, Owner, Target, Movement, PreVelocity,
		MaximumSlamSurfaceAngleDegrees, SlamGroundTraceHalfHeightCm,
		FVectorLiftForkMath::DirectedSlamUpgradeFloorCmPerSecond,
		RequestedSurfaceOverride,
		Solution))
	{
		if (!Solution.RequestedSurfacePoint.IsNearlyZero())
		{
			DrawDebugSphere(World, Solution.RequestedSurfacePoint, 55.0f, 20,
				FColor::Red, false, 0.8f, 0, 6.0f);
		}
		UE_LOG(LogVectorLiftFork, Log,
			TEXT("Directed slam REJECTED: target=%s reason=NO_FEASIBLE_ARC requested=%s preVelocity=%s budget=%.0f shrinkTried=0.9..0.1 velocityUnchanged=YES check=PASS"),
			*Target->GetName(), *Solution.RequestedSurfacePoint.ToCompactString(),
			*PreVelocity.ToCompactString(),
			FMath::Max(PreVelocity.Size(),
				FVectorLiftForkMath::DirectedSlamUpgradeFloorCmPerSecond));
		return false;
	}
	const FVector Start = Target->GetActorLocation();
	const FVectorDirectedSlamResult& SlamResult = Solution.Result;

	if (UVectorActionLockComponent* Lock =
		Owner->FindComponentByClass<UVectorActionLockComponent>())
	{
		if (!Lock->TryAcquire(this, TEXT("LiftForkSlam")))
		{
			UE_LOG(LogVectorLiftFork, Log,
				TEXT("Directed slam REJECTED: target=%s reason=ACTION_LOCK active=%s velocityUnchanged=YES"),
				*Target->GetName(), *Lock->GetActiveActionName().ToString());
			return false;
		}
		bOwnsActionLock = true;
	}
	// The lift and slam are one continuous action sentence. Restart this
	// component's timeline only after a valid solution and lock are secured.
	if (Timeline.IsBusy())
	{
		Timeline.Reset();
	}
	if (!Timeline.TryStartActive())
	{
		ReleaseActionLock();
		return false;
	}

	const bool bQueued = Movement->QueueAirborneWorldVelocityOverride(
		SlamResult.LaunchVelocity);
	if (!bQueued)
	{
		Timeline.Reset();
		ReleaseActionLock();
		UE_LOG(LogVectorLiftFork, Warning,
			TEXT("Directed slam REJECTED: target=%s reason=VELOCITY_QUEUE solved=%s velocityUnchanged=YES"),
			*Target->GetName(), *SlamResult.LaunchVelocity.ToCompactString());
		return false;
	}

	SlammedTargetsThisAirborneCycle.Add(TargetKey);
	if (UVectorImpactCollisionComponent* Impact =
		Target->FindComponentByClass<UVectorImpactCollisionComponent>())
	{
		Impact->ArmNextLandingSource(FName(TEXT("DIRECTED_SLAM")));
	}
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(Target))
	{
		Dummy->TriggerLiftForkPresentation();
	}
	DrawDebugSphere(World, Solution.RequestedSurfacePoint, 42.0f, 16,
		AutomaticClusterCount >= 0 ? FColor::Orange : FColor::Cyan,
		false, 0.8f, 0, 4.0f);
	DrawDebugSphere(World,
		Solution.ResolvedSurfacePoint + Solution.ResolvedSurfaceNormal * 4.0,
		58.0f, 20, FColor::Green, false, 0.8f, 0, 7.0f);
	DrawDebugDirectionalArrow(
		World, Start,
		Start + SlamResult.LaunchVelocity.GetSafeNormal() * 300.0,
		24.0f, FColor::Magenta, false, 0.8f, 0, 8.0f);
	if (Solution.ResolvedAlpha < 1.0)
	{
		DrawDebugLine(
			World, Solution.RequestedSurfacePoint, Solution.ResolvedSurfacePoint,
			FColor::Yellow, false, 0.8f, 0, 5.0f);
	}
	UE_LOG(LogVectorLiftFork, Log,
		TEXT("Directed slam queued: target=%s aimMode=%s clusterCount=%d start=%s requested=%s resolvedSurface=%s resolvedCenter=%s alpha=%.1f preVelocity=%s budget=%.0f flight=%.2f output=%s postVz=%.0f candidates=%d submission=ONCE landingSource=DIRECTED_SLAM check=%s"),
		*Target->GetName(), AutomaticClusterCount >= 0 ? TEXT("AUTO_DENSE") : TEXT("MANUAL_DRAG"),
		FMath::Max(0, AutomaticClusterCount), *Start.ToCompactString(),
		*Solution.RequestedSurfacePoint.ToCompactString(),
		*Solution.ResolvedSurfacePoint.ToCompactString(),
		*Solution.ResolvedTargetCenter.ToCompactString(),
		Solution.ResolvedAlpha,
		*PreVelocity.ToCompactString(), SlamResult.SpeedBudgetCmPerSecond,
		SlamResult.FlightSeconds, *SlamResult.LaunchVelocity.ToCompactString(),
		SlamResult.LaunchVelocity.Z, SlamResult.FeasibleCandidateCount,
		SlamResult.IsWithinDeclaredBudget() ? TEXT("PASS") : TEXT("FAIL"));
	return true;
}

void UVectorLiftForkComponent::RefreshAirborneCycleState()
{
	auto IsStillAirborne = [](AActor* Target)
	{
		const UVectorCharacterMovementComponent* Movement = Target
			? Target->FindComponentByClass<UVectorCharacterMovementComponent>() : nullptr;
		return Movement
			&& FVectorLiftForkMath::ShouldRouteToAirborneFollowUp(
				Movement->IsFalling(),
				Movement->GetEffectiveVelocityForPendingStep());
	};

	if (!IsStillAirborne(AirborneFollowUpTarget.Get()))
	{
		AirborneFollowUpTarget.Reset();
	}
	for (auto It = SlammedTargetsThisAirborneCycle.CreateIterator(); It; ++It)
	{
		if (!IsStillAirborne((*It).Get()))
		{
			It.RemoveCurrent();
		}
	}
}

void UVectorLiftForkComponent::CancelAction()
{
	bForkGestureHeld = false;
	ForkGestureHeldSeconds = 0.0;
	bForkGestureHasCursor = false;
	AirborneFollowUpTarget.Reset();
	Timeline.Reset();
	ReleaseActionLock();
}

void UVectorLiftForkComponent::ReleaseActionLock()
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
