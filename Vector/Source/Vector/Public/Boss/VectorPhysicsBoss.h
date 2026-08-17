// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Boss/VectorPhysicsBossState.h"
#include "Combat/VectorEnemy.h"
#include "VectorPhysicsBoss.generated.h"

class AVectorKineticOrb;
class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
enum class EVectorAnchorGroupSide : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FVectorPhysicsBossPhaseChangedSignature,
	EVectorPhysicsBossPhase, PreviousPhase,
	EVectorPhysicsBossPhase, CurrentPhase);

/** First playable physics Boss greybox: Magnet-Shell Mountain Beast. */
UCLASS()
class VECTOR_API AVectorPhysicsBoss : public AVectorEnemy
{
	GENERATED_BODY()

public:
	AVectorPhysicsBoss(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldPauseAI() const override;

	UFUNCTION(BlueprintPure, Category = "Vector|Boss")
	EVectorPhysicsBossPhase GetBossPhase() const { return BossState.GetPhase(); }

	UFUNCTION(BlueprintPure, Category = "Vector|Boss")
	FString GetBossStateDescription() const { return BossState.Describe(); }

	UFUNCTION(BlueprintPure, Category = "Vector|Boss")
	bool IsStaggerResolveActive() const { return BossState.IsStaggerResolveActive(); }

	/** True only during the launched horizontal ram, for kill attribution. */
	bool IsExecutingRam() const;

	UPROPERTY(BlueprintAssignable, Category = "Vector|Boss")
	FVectorPhysicsBossPhaseChangedSignature OnBossPhaseChanged;

	/** Persistent shell-state light, separate from attack and lift feedback. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Presentation")
	TObjectPtr<UPointLightComponent> BossPhaseLight;

	/** Collision-free core silhouette, revealed only after both shell anchors break. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Presentation")
	TObjectPtr<UStaticMeshComponent> ExposedCoreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Ram", meta = (ClampMin = "0.0", Units = "cm"))
	double RamTriggerRangeCm = 3000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Ram", meta = (ClampMin = "0.01", Units = "s"))
	double RamTelegraphSeconds = 0.85;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Ram", meta = (ClampMin = "0.0", Units = "cm/s"))
	double RamSpeedCmPerSecond = 1450.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Ram", meta = (ClampMin = "0.01", Units = "s"))
	double RamActiveSeconds = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Slam", meta = (ClampMin = "0.01", Units = "s"))
	double SlamTelegraphSeconds = 0.95;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Slam", meta = (ClampMin = "0.0", Units = "cm/s"))
	double SlamLaunchSpeedCmPerSecond = 850.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Slam", meta = (ClampMin = "0.1", Units = "s"))
	double SlamMaximumAirborneSeconds = 2.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AerialBurst", meta = (ClampMin = "0.01", Units = "s"))
	double AerialBurstTelegraphSeconds = 0.9;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AerialBurst", meta = (ClampMin = "0.0", Units = "cm"))
	double AerialBurstRadiusCm = 650.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AerialBurst", meta = (ClampMin = "0.0", Units = "cm/s"))
	double AerialBurstHorizontalBaseSpeedCmPerSecond = 900.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AerialBurst", meta = (ClampMin = "0.0", Units = "cm/s"))
	double AerialBurstVerticalBaseSpeedCmPerSecond = 1200.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AmmoLaunch", meta = (ClampMin = "0.01", Units = "s"))
	double AmmoLaunchTelegraphSeconds = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AmmoLaunch", meta = (ClampMin = "0.0", Units = "cm"))
	double AmmoSearchRadiusCm = 1600.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AmmoLaunch", meta = (ClampMin = "0.1", Units = "s"))
	double AmmoLaunchFlightSeconds = 1.2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|AmmoLaunch", meta = (ClampMin = "0.0", Units = "cm/s"))
	double AmmoLaunchMaximumBaseSpeedCmPerSecond = 2200.0;

	/** Reusable physical ammunition; prevents the fight becoming empty after adds die. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|KineticOrb")
	TSubclassOf<AVectorKineticOrb> KineticOrbClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|KineticOrb", meta = (ClampMin = "0.0", Units = "cm/s"))
	double KineticOrbLaunchSpeedCmPerSecond = 620.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|KineticOrb", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaximumActiveKineticOrbs = 5;

	/** Arena never permanently runs out of a physical answer to the shell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|KineticOrb", meta = (ClampMin = "0", ClampMax = "4"))
	int32 MinimumAvailableKineticOrbs = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|KineticOrb", meta = (ClampMin = "0.1", Units = "s"))
	double KineticOrbSupplyIntervalSeconds = 5.0;

	/** One stagger interrupts; this resolve window prevents a permanent gun stun loop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Stagger", meta = (ClampMin = "0.1", Units = "s"))
	double StaggerResolveSeconds = 4.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Stagger", meta = (ClampMin = "0.01", Units = "s"))
	double StaggerReactionSeconds = 0.45;

	/** Fallback for placed/non-PCG bosses; PCG may replace it with its authored safety plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Void", meta = (ClampMin = "100.0", Units = "cm"))
	double VoidRecoveryDepthBelowSpawnCm = 450.0;

private:
	enum class ERamPhase : uint8
	{
		Waiting,
		Telegraph,
		Active,
		SlamTelegraph,
		SlamAirborne,
		AerialBurstTelegraph,
		AmmoLaunchTelegraph,
		Recovery,
	};

	UFUNCTION()
	void HandleBossHealthChanged(double CurrentHealth, double MaximumHealth, double HealthDelta);

	UFUNCTION()
	void HandleBossStaggered();
	void HandleShellGroupBroken(EVectorAnchorGroupSide Side, int32 BrokenGroupCount);

	void BeginRamTelegraph();
	void LaunchRam();
	void BeginSlamTelegraph();
	void LaunchSlam();
	void BeginNextAttack();
	void BeginAerialBurstTelegraph();
	void ReleaseAerialBurst();
	bool BeginAmmoLaunchTelegraph();
	void LaunchAmmoTarget();
	void ClearAmmoTargetPresentation();
	AVectorEnemy* FindAmmoTarget() const;
	bool ComputeAmmoLaunchVelocity(AVectorEnemy* AmmoTarget, FVector& OutVelocity) const;
	bool SpawnKineticOrb(bool bLaunchTowardPlayer = true);
	int32 CountActiveKineticOrbs() const;
	void MaintainKineticOrbSupply(double DeltaSeconds);
	void AdvanceRam(double DeltaSeconds);
	void ApplyPhaseOutputs(EVectorPhysicsBossPhase PreviousPhase);
	void UpdateBossPresentation();
	APawn* FindPlayerPawn() const;

	FVectorPhysicsBossState BossState;
	ERamPhase RamPhase = ERamPhase::Waiting;
	double RamPhaseSecondsRemaining = 1.5;
	FVector LockedRamDirection = FVector::ForwardVector;
	int32 AttackSequenceIndex = 0;
	TWeakObjectPtr<AVectorEnemy> LockedAmmoTarget;
	FVector LockedAmmoAimPoint = FVector::ZeroVector;
	double KineticOrbSupplySecondsRemaining = 1.5;
	bool bStaggerCounterBurstPending = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
};
