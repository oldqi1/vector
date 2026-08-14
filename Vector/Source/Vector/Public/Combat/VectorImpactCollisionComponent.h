// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorImpactCollisionComponent.generated.h"

class AActor;
struct FHitResult;

/** 撞击硬表面的运行时事实：法向速度与本体反噬伤害。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FVectorWallImpactNativeSignature, double, double);
/** 身体接触事实；在伤害阈值判断前广播，供绳线等约束先行解除。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FVectorBodyImpactNativeSignature, AActor*);
/** 静态硬表面接触事实；低于伤害阈值也广播。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FVectorSurfaceContactNativeSignature, double);

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
	void OnCharacterImpact(const FHitResult& Hit, const FVector& MoveDelta);

	/** 落地回调（移动组件 OnMovementModeChanged 转发，Velocity.Z 为负的下落速度）。 */
	void OnLandedWithImpact(double FallSpeedCmPerSecond);

	/** 碰撞反应组件监听此事件；只广播事实，不在碰撞组件内嵌具体反应。 */
	FVectorWallImpactNativeSignature OnWallImpact;

	/** 身体碰撞监听；参数为本体接触到的另一个 Actor。 */
	FVectorBodyImpactNativeSignature OnBodyImpact;

	/** 静态硬表面接触监听；参数为法向闭合速度。 */
	FVectorSurfaceContactNativeSignature OnSurfaceContact;

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

	/** 碰撞恢复系数：0 完全非弹性，1 完全弹性；0.7 时等质量静止目标获得 85% 入射速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CollisionRestitution = 0.7;

	/** 没有稳定组件的对象（如玩家）的默认有效质量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Impact", meta = (ClampMin = "0.1"))
	double DefaultPhysicalMass = 2.5;

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
	void ResolveTargetCollision(
		AActor* TargetActor,
		const FVector& CollisionDirection,
		const FVector& StrikerVelocity,
		const FVector& TargetVelocity,
		double ClosingSpeedCmPerSecond);

	/** 结算"撞击者撞硬表面"的自反噬伤害。 */
	void ResolveSurfaceCollision(double ImpactSpeedCmPerSecond);

	/** 结算落地震荡 AOE（对半径内除自己外的稳定目标）。 */
	void ResolveLandingShock(double FallSpeedCmPerSecond);
};
