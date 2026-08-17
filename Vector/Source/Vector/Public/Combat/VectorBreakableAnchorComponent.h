// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorBreakableAnchorComponent.generated.h"

class AActor;
class UCharacterMovementComponent;
class UVectorStabilityComponent;

UENUM(BlueprintType)
enum class EVectorAnchorGroupSide : uint8
{
	Left,
	Right,
};

/** Result of one finite structure event. Kept POD-like so Automation can test it. */
struct VECTOR_API FVectorAnchorStructureResult
{
	bool bAccepted = false;
	bool bBrokeGroup = false;
	EVectorAnchorGroupSide Side = EVectorAnchorGroupSide::Left;
	int32 BrokenGroupCount = 0;
	FString Reason;
};

/** Two discrete paired anchor groups; generic damage never accumulates this ledger. */
struct VECTOR_API FVectorAnchorStructureLedger
{
	double MinimumClosingSpeedCmPerSecond = 720.0;
	double MinimumLateralAlignment = 0.55;
	bool bLeftBroken = false;
	bool bRightBroken = false;

	FVectorAnchorStructureResult ApplyCollisionEvent(
		double ClosingSpeedCmPerSecond,
		double SignedLateralAlignment);

	int32 GetBrokenGroupCount() const;
	bool IsGroupBroken(EVectorAnchorGroupSide Side) const;
	bool IsLaunchable() const { return bLeftBroken && bRightBroken; }
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FVectorAnchorGroupBrokenNativeSignature,
	EVectorAnchorGroupSide,
	int32);

/**
 * Discrete structure contract for the heavy Four-Anchor Beast.
 *
 * A lateral, finite body collision breaks the contacted paired anchor group.
 * Breaking groups changes real movement facts immediately; health damage and
 * generic repeated hammer hits never fill a hidden structure bar.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorBreakableAnchorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorBreakableAnchorComponent();

	virtual void BeginPlay() override;

	void SetStructureEnabled(bool bEnabled);

	FVectorAnchorStructureResult ApplyCollisionEvent(
		const FVector& CollisionDirectionTowardOwner,
		double ClosingSpeedCmPerSecond,
		AActor* EventSource);

	UFUNCTION(BlueprintPure, Category = "Vector|Structure")
	int32 GetBrokenGroupCount() const { return Ledger.GetBrokenGroupCount(); }

	UFUNCTION(BlueprintPure, Category = "Vector|Structure")
	bool IsLaunchable() const { return bStructureEnabled && Ledger.IsLaunchable(); }

	bool IsGroupBroken(EVectorAnchorGroupSide Side) const
	{
		return bStructureEnabled && Ledger.IsGroupBroken(Side);
	}

	UFUNCTION(BlueprintPure, Category = "Vector|Structure")
	bool IsStructureEnabled() const { return bStructureEnabled; }

	FVectorAnchorGroupBrokenNativeSignature OnAnchorGroupBroken;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MinimumClosingSpeedCmPerSecond = 720.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MinimumLateralAlignment = 0.55;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.1"))
	double AnchoredPhysicalMass = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.1"))
	double UnstablePhysicalMass = 3.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.1"))
	double LaunchablePhysicalMass = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.0"))
	double AnchoredGroundFriction = 1.4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.0"))
	double UnstableGroundFriction = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.0"))
	double LaunchableGroundFriction = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	double AnchoredBrakingDeceleration = 520.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	double UnstableBrakingDeceleration = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Structure|Response", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	double LaunchableBrakingDeceleration = 160.0;

private:
	void ApplyPhysicalResponse();

	FVectorAnchorStructureLedger Ledger;
	bool bStructureEnabled = false;
};
