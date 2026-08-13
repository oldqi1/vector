// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VectorTestDummy.h"
#include "VectorEnemy.generated.h"

/**
 * 敌人三型枚举（S04，灰盒期一个类三型配置，攻击行为分化后再拆子类）。
 */
UENUM(BlueprintType)
enum class EVectorEnemyArchetype : uint8
{
	/** 跳囊虫（轻）：成群、移动快、易失衡易击飞（质量 1.25）。 */
	LightHoppper,

	/** 甲壳犀（重）：移动慢、正面难推（质量 5.0），侧后方弱。 */
	HeavyRhinoBeetle,

	/** 角槌兽（冲）：追击中周期性冲锋（预警高亮 → 高速冲量），自身是"免费炮弹"。 */
	ChargerRammer,
};

/**
 * 灰盒敌人基类：测试靶全部能力（受控冲量/稳定/碰撞连锁/质量档）+ AI 追击。
 *
 * AI 由 AVectorEnemyController（NavMesh 寻路）驱动；失衡/倒地或被冲量驱动时
 * 暂停 AI（物理状态优先，玩家推出去的"弹药"不被 AI 拉回）。
 * 灰盒表现复用质量档配色：轻=绿小 / 重=紫大 / 冲锋=橙三角（Charger 特殊）。
 */
UCLASS()
class VECTOR_API AVectorEnemy : public AVectorTestDummy
{
	GENERATED_BODY()

public:
	AVectorEnemy(const FObjectInitializer& ObjectInitializer);

	/** 敌人三型配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy")
	EVectorEnemyArchetype Archetype = EVectorEnemyArchetype::LightHoppper;

	/** 常态移动速度（WalkSpeed），cm/s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MoveSpeedCmPerSecond = 300.0;

	/** 失衡/倒地/冲量驱动期间是否应暂停 AI（控制器在 Tick 中查询）。 */
	bool ShouldPauseAI() const;

protected:
	virtual void BeginPlay() override;

	/** 按三型应用配置（质量/速度/呈现），BeginPlay 幂等。 */
	void ApplyArchetypeConfiguration();

	/** 生命归零回调（灰盒期销毁；正式期替换为倒地/掉落表现）。 */
	UFUNCTION()
	void HandleDeath();
};
