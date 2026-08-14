// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Stability/VectorStabilityTypes.h"
#include "VectorTestDummy.generated.h"

class UStaticMeshComponent;
class UVectorStabilityComponent;
class UVectorImpactCollisionComponent;
class UVectorHealthComponent;
class UVectorWallBurstComponent;
class UVectorPhysicsModifierComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * 灰盒可推测试靶（冲量锤的试玩对象）。
 *
 * 用 ACharacter 承载（复用受控冲量移动 + 胶囊碰撞），挂 UVectorStabilityComponent
 * 提供稳定/失衡与质量三档，挂 UVectorImpactCollisionComponent 提供碰撞连锁结算
 * （S03：被推后高速撞墙/撞其他靶子产生伤害）。S04 敌人三型直接以此为基础扩展。
 * 灰盒表现：纯色方块，颜色/大小随质量档（轻=绿小 / 中=橙中 / 重=紫大）。
 */
UCLASS()
class VECTOR_API AVectorTestDummy : public ACharacter
{
	GENERATED_BODY()

public:
	AVectorTestDummy(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 敌人攻击前摇表现开关；与失衡表现共享同一份缓存动态材质。 */
	void SetAttackWarningPresentation(bool bActive);

	/** 调质器颜色状态：润滑=蓝、浮空=青、同时存在=亮青。 */
	void SetPhysicsModifierPresentation(bool bLubricated, bool bBuoyant);

	/** 质量三档：决定被推难度与稳定度/碰撞系数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	EVectorMassClass MassClass = EVectorMassClass::Medium;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorStabilityComponent> StabilityComponent;

	/** 碰撞连锁结算（S03）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorImpactCollisionComponent> ImpactCollisionComponent;

	/** 高速撞墙时触发一次范围冲击。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorWallBurstComponent> WallBurstComponent;

	/** 核心生命（S05 击杀层）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorPhysicsModifierComponent> PhysicsModifierComponent;

	/** 灰盒占位方块（纯色，随质量档变色/变尺寸）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy|Presentation")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 攻击前摇点光源：不依赖方块材质参数，保证灰盒预警在场景中清晰可见。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy|Presentation")
	TObjectPtr<UPointLightComponent> AttackWarningLight;

protected:
	/** 按质量档应用方块颜色与缩放（派生类可在质量档变更后重调）。 */
	virtual void ApplyMassPresentation();

	/** 按稳定度状态更新失衡表现（失衡白闪 / 倒地躺平 / 恢复直立），每帧调用。 */
	void UpdateStaggerPresentation();

	/** 质量档基础颜色（失衡表现叠加用，恢复时还原）。 */
	FLinearColor BaseBodyColor = FLinearColor::White;

	/** 质量档基础尺寸；预警脉冲结束后精确还原。 */
	FVector BaseBodyScale = FVector(0.9f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	bool bAttackWarningActive = false;
	bool bLubricatedPresentation = false;
	bool bBuoyantPresentation = false;
};
