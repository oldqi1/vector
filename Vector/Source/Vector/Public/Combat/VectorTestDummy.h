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

	/** 质量三档：决定被推难度与稳定度/碰撞系数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	EVectorMassClass MassClass = EVectorMassClass::Medium;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorStabilityComponent> StabilityComponent;

	/** 碰撞连锁结算（S03）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorImpactCollisionComponent> ImpactCollisionComponent;

	/** 核心生命（S05 击杀层）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy")
	TObjectPtr<UVectorHealthComponent> HealthComponent;

	/** 灰盒占位方块（纯色，随质量档变色/变尺寸）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|TestDummy|Presentation")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

protected:
	/** 按质量档应用方块颜色与缩放（派生类可在质量档变更后重调）。 */
	virtual void ApplyMassPresentation();

	/** 按稳定度状态更新失衡表现（失衡白闪 / 倒地躺平 / 恢复直立），每帧调用。 */
	void UpdateStaggerPresentation();

	/** 质量档基础颜色（失衡表现叠加用，恢复时还原）。 */
	FLinearColor BaseBodyColor = FLinearColor::White;
};
