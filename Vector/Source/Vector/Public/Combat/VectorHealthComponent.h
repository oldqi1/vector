// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorHealthComponent.generated.h"

/** 生命归零时广播（灰盒期绑定死亡表现，如销毁）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVectorHealthDeathSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVectorHealthChangedSignature,
	double, CurrentHealth,
	double, MaximumHealth,
	double, HealthDelta);

/**
 * 核心生命组件（S05：敌人的"击杀层"）。
 *
 * 与稳定度（控制层：失衡/倒地）分离：稳定度归零只失衡，生命归零才死亡。
 * 纯 C++ 账本（ApplyDamage/IsDead/Reset），无 World 依赖，Automation 可测。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorHealthComponent();
	virtual void BeginPlay() override;

	/**
	 * 施加伤害（扣核心生命）。
	 *
	 * @param Amount 伤害量；非有限或 ≤0 返回 false 且无副作用。
	 * @return true 表示本次伤害致死（生命归零，触发 OnDeath）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Vector|Health")
	bool ApplyDamage(double Amount);

	/** 生命归零后为 true（后续伤害不再叠加）。 */
	UFUNCTION(BlueprintPure, Category = "Vector|Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Vector|Health")
	double GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Vector|Health")
	double GetMaxHealth() const { return MaxHealth; }

	/** 重置到满血（灰盒重开/复活用）。 */
	UFUNCTION(BlueprintCallable, Category = "Vector|Health")
	void ResetHealth();

	/** 生命归零广播（只读事实，不承载表现）。 */
	UPROPERTY(BlueprintAssignable, Category = "Vector|Health")
	FVectorHealthDeathSignature OnDeath;

	/** 生命变化广播；HealthDelta < 0 表示受伤，> 0 表示恢复。 */
	UPROPERTY(BlueprintAssignable, Category = "Vector|Health")
	FVectorHealthChangedSignature OnHealthChanged;

	/** 核心生命上限（对齐原项目核心 100 档）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Health", meta = (ClampMin = "1.0"))
	double MaxHealth = 100.0;

private:
	/** 当前核心生命。 */
	double CurrentHealth = 100.0;

	/** 是否已死亡（防重复触发 OnDeath）。 */
	bool bDead = false;
};
