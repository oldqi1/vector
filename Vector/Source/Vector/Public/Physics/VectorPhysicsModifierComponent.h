// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorPhysicsModifierComponent.generated.h"

class UCharacterMovementComponent;

/** 统一维护环境与限时调质器对 CharacterMovement 的物理属性修改。 */
UCLASS(ClassGroup = (Vector), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorPhysicsModifierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorPhysicsModifierComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ApplyLubricant(double DurationSeconds = -1.0);
	void ApplyBuoyantSpore(double DurationSeconds = -1.0);

	/** 环境区层；1 为正常，越小越滑。 */
	void SetEnvironmentFrictionMultiplier(double Multiplier);
	void ClearEnvironmentFrictionMultiplier();

	UFUNCTION(BlueprintPure, Category = "Vector|PhysicsModifier")
	bool IsLubricated() const { return LubricantSecondsRemaining > 0.0; }

	UFUNCTION(BlueprintPure, Category = "Vector|PhysicsModifier")
	bool IsBuoyant() const { return BuoyantSecondsRemaining > 0.0; }

	UFUNCTION(BlueprintPure, Category = "Vector|PhysicsModifier")
	double GetLubricantSecondsRemaining() const { return LubricantSecondsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vector|PhysicsModifier")
	double GetBuoyantSecondsRemaining() const { return BuoyantSecondsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vector|PhysicsModifier")
	bool IsEnvironmentFrictionModified() const { return EnvironmentFrictionMultiplier < 0.999; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PhysicsModifier|Lubricant", meta = (ClampMin = "0.0", Units = "s"))
	double LubricantDurationSeconds = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PhysicsModifier|Lubricant", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double LubricantFrictionMultiplier = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PhysicsModifier|Buoyant", meta = (ClampMin = "0.0", Units = "s"))
	double BuoyantDurationSeconds = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|PhysicsModifier|Buoyant", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	double BuoyantGravityMultiplier = 0.35;

private:
	void CaptureBaseline();
	void ApplyEffectiveSettings();
	void UpdatePresentation() const;

	TWeakObjectPtr<UCharacterMovementComponent> Movement;
	bool bBaselineCaptured = false;
	float BaseGroundFriction = 0.0f;
	float BaseBrakingFriction = 0.0f;
	float BaseBrakingFrictionFactor = 0.0f;
	float BaseBrakingDeceleration = 0.0f;
	float BaseGravityScale = 1.0f;
	bool bBaseUseSeparateBrakingFriction = false;

	double EnvironmentFrictionMultiplier = 1.0;
	double LubricantSecondsRemaining = 0.0;
	double BuoyantSecondsRemaining = 0.0;
};
