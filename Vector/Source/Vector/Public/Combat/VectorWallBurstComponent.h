// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorWallBurstComponent.generated.h"

/** 首次高速撞墙触发的范围冲击反应。 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorWallBurstComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorWallBurstComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|WallBurst", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MinimumTriggerSpeedCmPerSecond = 800.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|WallBurst", meta = (ClampMin = "0.0", Units = "cm"))
	double RadiusCm = 400.0;

	/** 范围推力基值，实际目标速度按目标有效质量相除。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|WallBurst", meta = (ClampMin = "0.0", Units = "cm/s"))
	double ImpulseBaseSpeedCmPerSecond = 1400.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|WallBurst", meta = (ClampMin = "0.0"))
	double DamagePerSpeed = 0.04;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|WallBurst", meta = (ClampMin = "0.0"))
	double MaximumDamage = 40.0;

	/** 防止贴墙多次 HandleImpact 在短时间内重复爆发。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|WallBurst", meta = (ClampMin = "0.0", Units = "s"))
	double CooldownSeconds = 0.75;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleWallImpact(double ImpactSpeedCmPerSecond, double SelfDamage);
	double LastBurstWorldSeconds = -1.e9;
};
