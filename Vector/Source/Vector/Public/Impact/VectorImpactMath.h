// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 冲量荒原碰撞数学纯函数集（A3 平面化移植自 MorphorbitImpactBladeComponent）。
 *
 * 全部为无 World 依赖的静态函数，运行时与 Automation 共用同一公式。
 * 平面化要点（红线 DEBT-02）：局部朝上恒为世界 +Z，不再从 GravityCenter 推导；
 * 相对速度只取水平分量，垂直分量由标准 CharacterMovement 处理。
 */
struct VECTOR_API FVectorImpactMath
{
	/**
	 * 计算正向水平闭合速度，单位为 cm/s，截断到 [0, 300]。
	 *
	 * @param PlayerLocation  施力/攻击方世界位置，单位为 cm。
	 * @param PlayerVelocity  施力/攻击方世界速度，单位为 cm/s。
	 * @param TargetVelocity  目标世界速度，单位为 cm/s。
	 * @param ImpactPoint     世界空间接触点，单位为 cm。
	 * @return 沿"玩家→接触点"水平方向的闭合速度；无效或非闭合输入返回零。
	 */
	static double ComputeClosingSpeedCmPerSecond(
		const FVector& PlayerLocation,
		const FVector& PlayerVelocity,
		const FVector& TargetVelocity,
		const FVector& ImpactPoint);

	/**
	 * 把有限闭合速度转换为基础失衡量，范围为 [20, 30]。
	 *
	 * 0 cm/s → 20；290 cm/s（全速阈值）及以上 → 30；中间按 300 分母线性。
	 * 非有限或负值按 0 处理。乘数（弱点/质量/碰撞类型）由调用方叠加。
	 */
	static double ComputeStaggerDamage(double ClosingSpeedCmPerSecond);

	/**
	 * 高速碰撞伤害（设计案 v0.1"碰撞伤害"公式的落地）：
	 * 超过阈值的速度 × 每速伤害 × 质量系数 × 碰撞类型系数，最终设上限。
	 *
	 * 只有速度严格大于 MinDamageSpeed 才产生伤害；所有乘数为负或非有限时按
	 * 零处理；结果 clamp 到 [0, MaxDamage]，防止一次碰撞数值溢出。
	 *
	 * @param RelativeSpeedCmPerSecond 相对速度（沿撞击方向的闭合分量），cm/s。
	 * @param MassMultiplier           被撞目标的质量系数（轻/中/重查表）。
	 * @param CollisionTypeMultiplier  碰撞类型系数（Wall=撞墙 / Body=撞人 / Ground=落地）。
	 * @param MinDamageSpeedCmPerSecond 产生伤害的最小速度阈值，默认 300 cm/s。
	 * @param DamagePerSpeed           每 1 cm/s 超速的稳定度伤害，默认 0.05。
	 * @param MaxDamage                单次碰撞伤害上限，默认 50。
	 * @return 实际碰撞稳定度伤害；速度低于阈值返回 0。
	 */
	static double ComputeCollisionDamage(
		double RelativeSpeedCmPerSecond,
		double MassMultiplier = 1.0,
		double CollisionTypeMultiplier = 1.0,
		double MinDamageSpeedCmPerSecond = 300.0,
		double DamagePerSpeed = 0.05,
		double MaxDamage = 50.0);
};
