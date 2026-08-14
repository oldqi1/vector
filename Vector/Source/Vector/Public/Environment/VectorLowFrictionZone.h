// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorLowFrictionZone.generated.h"

class UBoxComponent;
class UCharacterMovementComponent;
class UVectorPhysicsModifierComponent;

/**
 * 低摩擦环境区。
 *
 * CharacterMovement 不直接采用地面 PhysicalMaterial 的刚体摩擦，因此灰盒区域通过重叠
 * 临时修改移动组件的抓地/制动参数；离开和销毁时完整恢复进入前数值。
 */
UCLASS()
class VECTOR_API AVectorLowFrictionZone : public AActor
{
	GENERATED_BODY()

public:
	AVectorLowFrictionZone();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Friction")
	TObjectPtr<UBoxComponent> ZoneBounds;

	/** 统一属性组件存在时使用的基准摩擦倍率。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Friction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ZoneFrictionMultiplier = 0.10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Friction|Debug")
	bool bDrawDebugBounds = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Friction", meta = (ClampMin = "0.0"))
	float ZoneGroundFriction = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Friction", meta = (ClampMin = "0.0"))
	float ZoneBrakingFriction = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Friction", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float ZoneBrakingDeceleration = 40.0f;

protected:
	virtual void BeginPlay() override;

private:
	struct FMovementSettings
	{
		float GroundFriction = 0.0f;
		float BrakingFriction = 0.0f;
		float BrakingFrictionFactor = 0.0f;
		float BrakingDecelerationWalking = 0.0f;
		bool bUseSeparateBrakingFriction = false;
	};

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void ApplyLowFriction(UCharacterMovementComponent* Movement);
	void RestoreMovement(UCharacterMovementComponent* Movement);

	TMap<TWeakObjectPtr<UCharacterMovementComponent>, FMovementSettings> OriginalSettings;
	TSet<TWeakObjectPtr<UVectorPhysicsModifierComponent>> ActiveModifierComponents;
};
