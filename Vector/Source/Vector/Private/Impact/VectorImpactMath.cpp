// Copyright Epic Games, Inc. All Rights Reserved.

#include "Impact/VectorImpactMath.h"

namespace VectorImpactMathInternal
{
	/** 移植自 MorphorbitImpactBladeComponent.cpp L22-27。 */
	constexpr double MaximumClosingSpeedCmPerSecond = 300.0;
	constexpr double FullSpeedClosingThresholdCmPerSecond = 290.0;
	constexpr double BaseStaggerDamage = 20.0;
	constexpr double MaximumBonusStaggerDamage = 10.0;

	bool IsFiniteVector(const FVector& Vector)
	{
		return !Vector.ContainsNaN()
			&& FMath::IsFinite(Vector.X)
			&& FMath::IsFinite(Vector.Y)
			&& FMath::IsFinite(Vector.Z);
	}
}

double FVectorImpactMath::ComputeClosingSpeedCmPerSecond(
	const FVector& PlayerLocation,
	const FVector& PlayerVelocity,
	const FVector& TargetVelocity,
	const FVector& ImpactPoint)
{
	using namespace VectorImpactMathInternal;

	if (!IsFiniteVector(PlayerLocation)
		|| !IsFiniteVector(PlayerVelocity)
		|| !IsFiniteVector(TargetVelocity)
		|| !IsFiniteVector(ImpactPoint))
	{
		return 0.0;
	}

	// 平面化（红线 DEBT-02）：局部朝上恒为 +Z，水平面即"切平面"。
	const FVector ImpactUp = FVector::UpVector;
	const FVector PlayerToImpact = FVector::VectorPlaneProject(ImpactPoint - PlayerLocation, ImpactUp);
	if (PlayerToImpact.IsNearlyZero())
	{
		return 0.0;
	}

	const FVector ClosingDirection = PlayerToImpact.GetSafeNormal();
	const FVector RelativeTangentVelocity = FVector::VectorPlaneProject(
		PlayerVelocity - TargetVelocity,
		ImpactUp);
	const double ClosingSpeed = FVector::DotProduct(RelativeTangentVelocity, ClosingDirection);
	return FMath::IsFinite(ClosingSpeed)
		? FMath::Clamp(ClosingSpeed, 0.0, MaximumClosingSpeedCmPerSecond)
		: 0.0;
}

double FVectorImpactMath::ComputeStaggerDamage(const double ClosingSpeedCmPerSecond)
{
	using namespace VectorImpactMathInternal;

	const double FiniteSpeed = FMath::IsFinite(ClosingSpeedCmPerSecond)
		? FMath::Max(0.0, ClosingSpeedCmPerSecond)
		: 0.0;
	const double NormalizedSpeed = FiniteSpeed >= FullSpeedClosingThresholdCmPerSecond
		? 1.0
		: FMath::Clamp(FiniteSpeed / MaximumClosingSpeedCmPerSecond, 0.0, 1.0);
	return BaseStaggerDamage + MaximumBonusStaggerDamage * FMath::Clamp(
		NormalizedSpeed,
		0.0,
		1.0);
}

double FVectorImpactMath::ComputeCollisionDamage(
	const double RelativeSpeedCmPerSecond,
	const double MassMultiplier,
	const double CollisionTypeMultiplier,
	const double MinDamageSpeedCmPerSecond,
	const double DamagePerSpeed,
	const double MaxDamage)
{
	// 速度必须有限且严格超过阈值；非有限阈值按零处理（安全兜底）。
	if (!FMath::IsFinite(RelativeSpeedCmPerSecond)
		|| !FMath::IsFinite(MinDamageSpeedCmPerSecond)
		|| !FMath::IsFinite(DamagePerSpeed)
		|| !FMath::IsFinite(MaxDamage)
		|| !FMath::IsFinite(MassMultiplier)
		|| !FMath::IsFinite(CollisionTypeMultiplier)
		|| RelativeSpeedCmPerSecond <= MinDamageSpeedCmPerSecond)
	{
		return 0.0;
	}

	// 只对"超过阈值"的速度部分计伤：速度刚过阈值时伤害从 0 平滑增长。
	const double SpeedDamage =
		(RelativeSpeedCmPerSecond - MinDamageSpeedCmPerSecond)
		* FMath::Max(0.0, DamagePerSpeed);
	const double TotalDamage =
		SpeedDamage
		* FMath::Max(0.0, MassMultiplier)
		* FMath::Max(0.0, CollisionTypeMultiplier);

	return FMath::Clamp(TotalDamage, 0.0, FMath::Max(0.0, MaxDamage));
}
