// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorImpactCollisionComponent.generated.h"

struct FHitResult;

/**
 * 碰撞连锁结算组件（S03，挂载所有可成为"弹药"的目标：测试靶/未来敌人）。
 *
 * 由 UVectorCharacterMovementComponent 在冲量驱动的高速碰撞（HandleImpact）
 * 与落地（OnMovementModeChanged）时调用，完成：
 * - 撞到其他目标：对方扣碰撞伤害 + 传递部分冲量（动量传递雏形）；
 * - 撞到硬表面（墙/地面）：自己扣反噬伤害（撞墙语义）；
 * - 高速落地：对周围目标造成落地震荡 AOE。
 *
 * 伤害公式统一走 FVectorImpactMath::ComputeCollisionDamage
 * （相对速度 × 质量系数 × 碰撞类型系数，硬上限），保证可复算（验收 #1）。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorImpactCollisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorImpactCollisionComponent();

	/** 冲量驱动碰撞回调（移动组件 HandleImpact 转发）。 */
	void OnCharacterImpact(const FHitResult& Hit, double ImpactSpeedCmPerSecond, const FVector& MoveDelta);

	/** 落地回调（移动组件 OnMovementModeChanged 转发，Velocity.Z 为负的下落速度）。 */
	void OnLandedWithImpact(double FallSpeedCmPerSecond);

	// ---- 伤害数值（Details 可调） ----

	/** 产生碰撞伤害的最小速度，cm/s（低于此速度的碰撞无伤害）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MinDamageSpeedCmPerSecond = 300.0;

	/** 每 1 cm/s 超速的稳定度伤害。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0"))
	double DamagePerSpeed = 0.05;

	/** 单次碰撞伤害上限（防止一次数值溢出清空目标）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0"))
	double MaxDamage = 50.0;

	/** 撞墙（硬表面）碰撞类型系数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0"))
	double WallCollisionMultiplier = 1.5;

	/** 撞到其他目标（身体互撞）碰撞类型系数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0"))
	double BodyCollisionMultiplier = 1.0;

	/** 撞击到其他目标时传递的速度比例（动量传递雏形）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MomentumTransferRatio = 0.4;

	/** 落地震荡触发的最小下落速度，cm/s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact|Landing", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MinFallSpeedCmPerSecond = 600.0;

	/** 落地震荡影响半径，cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact|Landing", meta = (ClampMin = "0.0", Units = "cm"))
	double LandedAoERadiusCm = 400.0;

	/** 落地震荡是否启用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact|Landing")
	bool bEnableLandingShock = true;

private:
	/** 结算"撞击者 → 目标"的碰撞伤害与冲量传递。 */
	void ResolveTargetCollision(AActor* TargetActor, const FVector& MoveDirection, double ImpactSpeedCmPerSecond);

	/** 结算"撞击者撞硬表面"的自反噬伤害。 */
	void ResolveSurfaceCollision(double ImpactSpeedCmPerSecond);

	/** 结算落地震荡 AOE（对半径内除自己外的稳定目标）。 */
	void ResolveLandingShock(double FallSpeedCmPerSecond);
};
