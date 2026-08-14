// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorEnemyAttackComponent.generated.h"

/**
 * 敌人对玩家的近身攻击（P2 补全：让三种怪从"追人"变"会打人"）。
 *
 * 灰盒期统一形态（对齐设计案"攻击前显示预警，动作可读"）：
 *   进入攻击范围 → 预警停顿（白闪，可读）→ 向玩家方向扑击（短距冲量）→ 冷却。
 * 扑击命中玩家：伤害与双方碰后速度统一由 VectorImpactCollisionComponent 按
 * 相对速度、双方质量与恢复系数结算，攻击组件不再重复扣一次固定伤害。
 *
 * 注意：不套用共享 FVectorActionTimeline（它是"输入驱动蓄力"语义，敌人攻击是
 * 自计时预警），组件内自管 Windup/Active/Cooldown 三个计时，命名用 AttackPhase
 * 避免与装备动作阶段枚举混淆（DEBT-01 红线：不新增全局装备 Phase 枚举）。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorEnemyAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorEnemyAttackComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ---- 攻击数值（Details 可调） ----

	/** 触发扑击的玩家距离，cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|EnemyAttack", meta = (ClampMin = "0.0", Units = "cm"))
	double AttackTriggerRangeCm = 250.0;

	/** 预警时长（可读停顿），s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|EnemyAttack", meta = (ClampMin = "0.0", Units = "s"))
	double WarmupSeconds = 0.35;

	/** 扑击冲量速度，cm/s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|EnemyAttack", meta = (ClampMin = "0.0", Units = "cm/s"))
	double PounceSpeedCmPerSecond = 800.0;

	/** 扑击后冷却，s（防无限连啃）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|EnemyAttack", meta = (ClampMin = "0.0", Units = "s"))
	double CooldownSeconds = 1.5;

	/** 攻击进行中（预警/扑击/冷却），供 AI 让路判断。 */
	bool IsAttacking() const { return bAttacking; }

private:
	/** 自管攻击阶段（区别于装备动作阶段枚举）。 */
	enum class EAttackPhase : uint8
	{
		Idle,
		Warmup,   // 预警停顿（可读）
		Pouncing, // 扑击冲量中
		Cooldown, // 冷却
	};

	EAttackPhase AttackPhase = EAttackPhase::Idle;

	/** 当前阶段剩余时长，s。 */
	double PhaseSecondsRemaining = 0.0;

	/** 预警开始时锁定的扑击方向（朝向玩家）。 */
	FVector PounceDirection = FVector::ForwardVector;

	/** 攻击进行中（预警/扑击/冷却任一阶段）。 */
	bool bAttacking = false;

	/** 找到当前世界玩家 Pawn。 */
	class APawn* FindPlayerPawn() const;

	/** 进入预警阶段。 */
	void BeginWarmup();

	/** 预警结束执行扑击（对玩家方向施加冲量）。 */
	void ExecutePounce();

	/** 灰盒预警表现：预警时白色高亮（可读性，敌人"要打你了"）。 */
	void UpdateWarmupPresentation(bool bWarmup);
};
