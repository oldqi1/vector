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
	/** 把速度/冲量强度按统一有效质量换算为目标速度；非法输入返回零。 */
	static double ComputeMassAdjustedSpeed(double BaseSpeedCmPerSecond, double EffectiveMass);

	/**
	 * 计算沿给定水平碰撞方向的闭合速度，不设上限。
	 * Direction 指向“撞击者 → 被撞者”；双方正在分离时返回 0。
	 */
	static double ComputePlanarClosingSpeed(
		const FVector& StrikerVelocity,
		const FVector& TargetVelocity,
		const FVector& Direction);

	/**
	 * 一维双物体碰撞解算（动量守恒 + 恢复系数）。
	 *
	 * @param StrikerSpeed 撞击者沿碰撞方向的碰前速度。
	 * @param TargetSpeed 被撞者沿碰撞方向的碰前速度。
	 * @param StrikerMass 撞击者有效质量，必须 > 0。
	 * @param TargetMass 被撞者有效质量，必须 > 0。
	 * @param Restitution 恢复系数 [0,1]；0 完全非弹性，1 完全弹性。
	 * @return 输入有效时返回 true，并写出双方碰后速度；无效输入返回 false。
	 */
	static bool SolveOneDimensionalCollision(
		double StrikerSpeed,
		double TargetSpeed,
		double StrikerMass,
		double TargetMass,
		double Restitution,
		double& OutStrikerSpeed,
		double& OutTargetSpeed);

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
	 * @param MassMultiplier           撞击者的伤害质量系数（轻/中/重查表）。
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
