// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Boss/VectorPhysicsBossState.h"
#include "Combat/VectorEnemy.h"
#include "VectorPhysicsBoss.generated.h"

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

	UPROPERTY(BlueprintAssignable, Category = "Vector|Boss")
	FVectorPhysicsBossPhaseChangedSignature OnBossPhaseChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Boss|Ram", meta = (ClampMin = "0.0", Units = "cm"))
	double RamTriggerRangeCm = 1800.0;

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

private:
	enum class ERamPhase : uint8
	{
		Waiting,
		Telegraph,
		Active,
		SlamTelegraph,
		SlamAirborne,
		Recovery,
	};

	UFUNCTION()
	void HandleBossHealthChanged(double CurrentHealth, double MaximumHealth, double HealthDelta);

	UFUNCTION()
	void HandleBossStaggered();

	void BeginRamTelegraph();
	void LaunchRam();
	void BeginSlamTelegraph();
	void LaunchSlam();
	void AdvanceRam(double DeltaSeconds);
	void ApplyPhaseOutputs(EVectorPhysicsBossPhase PreviousPhase);
	void UpdateBossPresentation();
	APawn* FindPlayerPawn() const;

	FVectorPhysicsBossState BossState;
	ERamPhase RamPhase = ERamPhase::Waiting;
	double RamPhaseSecondsRemaining = 1.5;
	FVector LockedRamDirection = FVector::ForwardVector;
	bool bUseSlamForNextAttack = false;
};
