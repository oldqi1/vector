// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 双端绳线枪纯数学；运行时与 Automation 共用，避免质量/约束规则分叉。 */
namespace FVectorGravityHookMath
{
	inline bool IsFiniteVector(const FVector& Value)
	{
		return !Value.ContainsNaN()
			&& FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	/** 平面共同质心；质量非法时返回零向量。 */
	inline FVector ComputePlanarCenterOfMass(
		const FVector& PositionA,
		const FVector& PositionB,
		const double MassA,
		const double MassB)
	{
		if (!IsFiniteVector(PositionA) || !IsFiniteVector(PositionB)
			|| !FMath::IsFinite(MassA) || !FMath::IsFinite(MassB)
			|| MassA <= 0.0 || MassB <= 0.0)
		{
			return FVector::ZeroVector;
		}
		const double TotalMass = MassA + MassB;
		return FVector(
			(PositionA.X * MassA + PositionB.X * MassB) / TotalMass,
			(PositionA.Y * MassA + PositionB.Y * MassB) / TotalMass,
			(PositionA.Z + PositionB.Z) * 0.5);
	}

	/** 相对运动绕共同质心的单位质量角动量 h = (r × vRel).Z。 */
	inline double ComputePlanarSpecificAngularMomentum(
		const FVector& PositionA,
		const FVector& PositionB,
		const FVector& VelocityA,
		const FVector& VelocityB)
	{
		if (!IsFiniteVector(PositionA) || !IsFiniteVector(PositionB)
			|| !IsFiniteVector(VelocityA) || !IsFiniteVector(VelocityB))
		{
			return 0.0;
		}
		const FVector Separation = FVector::VectorPlaneProject(
			PositionB - PositionA, FVector::UpVector);
		const FVector RelativeVelocity = FVector::VectorPlaneProject(
			VelocityB - VelocityA, FVector::UpVector);
		return FVector::CrossProduct(Separation, RelativeVelocity).Z;
	}

	/**
	 * Solve a one-sided, non-elastic tether.
	 *
	 * A slack rope does nothing. A taut rope cancels only outward separation;
	 * inward motion is never pushed back. A positive reel speed shortens a taut
	 * rope without spring oscillation. The center-of-mass momentum is preserved.
	 */
	inline bool SolveTetheredPairVelocities(
		const FVector& PositionA,
		const FVector& PositionB,
		const FVector& VelocityA,
		const FVector& VelocityB,
		const double MassA,
		const double MassB,
		const double CableLengthCm,
		const double ReelSpeedCmPerSecond,
		const double ConstraintToleranceCm,
		const double DeltaTimeSeconds,
		const double SpecificAngularMomentumCm2PerSecond,
		const double MaxRelativeTangentialSpeedCmPerSecond,
		const double MaxCorrectionSpeedCmPerSecond,
		FVector& OutVelocityA,
		FVector& OutVelocityB,
		bool& bOutTaut)
	{
		OutVelocityA = FVector::ZeroVector;
		OutVelocityB = FVector::ZeroVector;
		bOutTaut = false;
		if (!IsFiniteVector(PositionA) || !IsFiniteVector(PositionB)
			|| !IsFiniteVector(VelocityA) || !IsFiniteVector(VelocityB)
			|| !FMath::IsFinite(MassA) || !FMath::IsFinite(MassB)
			|| !FMath::IsFinite(CableLengthCm)
			|| !FMath::IsFinite(ReelSpeedCmPerSecond)
			|| !FMath::IsFinite(ConstraintToleranceCm)
			|| !FMath::IsFinite(DeltaTimeSeconds)
			|| !FMath::IsFinite(SpecificAngularMomentumCm2PerSecond)
			|| !FMath::IsFinite(MaxRelativeTangentialSpeedCmPerSecond)
			|| !FMath::IsFinite(MaxCorrectionSpeedCmPerSecond)
			|| MassA <= 0.0 || MassB <= 0.0)
		{
			return false;
		}

		const FVector Separation = FVector::VectorPlaneProject(
			PositionB - PositionA, FVector::UpVector);
		const double Distance = Separation.Size();
		if (Distance <= UE_SMALL_NUMBER)
		{
			return false;
		}

		const double SafeCableLength = FMath::Max(0.0, CableLengthCm);
		const double SafeTolerance = FMath::Max(0.0, ConstraintToleranceCm);
		if (Distance < SafeCableLength - SafeTolerance)
		{
			// A rope cannot push its endpoints apart. Preserve completely free motion
			// until the slack has been consumed.
			OutVelocityA = VelocityA;
			OutVelocityB = VelocityB;
			return true;
		}

		bOutTaut = true;
		const FVector Radial = Separation / Distance;
		const FVector Tangent(-Radial.Y, Radial.X, 0.0);
		const FVector CurrentRelativeVelocity = FVector::VectorPlaneProject(
			VelocityB - VelocityA, FVector::UpVector);
		const double CurrentRadialSeparationSpeed = FVector::DotProduct(
			CurrentRelativeVelocity, Radial);
		const double StretchCm = FMath::Max(0.0, Distance - SafeCableLength);
		const double SafeDeltaTime = FMath::Max(
			static_cast<double>(UE_SMALL_NUMBER), DeltaTimeSeconds);
		const double CorrectionSpeed = FMath::Min(
			FMath::Max(0.0, MaxCorrectionSpeedCmPerSecond),
			StretchCm / SafeDeltaTime);
		const double DesiredRadialSeparationSpeed = FMath::Min(
			CurrentRadialSeparationSpeed,
			-FMath::Max(0.0, ReelSpeedCmPerSecond) - CorrectionSpeed);
		const double RelativeTangentialSpeed = FMath::Clamp(
			SpecificAngularMomentumCm2PerSecond / Distance,
			-FMath::Max(0.0, MaxRelativeTangentialSpeedCmPerSecond),
			FMath::Max(0.0, MaxRelativeTangentialSpeedCmPerSecond));
		const FVector DesiredRelativeVelocity =
			Radial * DesiredRadialSeparationSpeed + Tangent * RelativeTangentialSpeed;

		const double TotalMass = MassA + MassB;
		const FVector PlanarVelocityA = FVector::VectorPlaneProject(VelocityA, FVector::UpVector);
		const FVector PlanarVelocityB = FVector::VectorPlaneProject(VelocityB, FVector::UpVector);
		const FVector CenterOfMassVelocity =
			(PlanarVelocityA * MassA + PlanarVelocityB * MassB) / TotalMass;
		OutVelocityA = CenterOfMassVelocity
			- DesiredRelativeVelocity * (MassB / TotalMass);
		OutVelocityB = CenterOfMassVelocity
			+ DesiredRelativeVelocity * (MassA / TotalMass);
		OutVelocityA.Z = VelocityA.Z;
		OutVelocityB.Z = VelocityB.Z;
		return IsFiniteVector(OutVelocityA) && IsFiniteVector(OutVelocityB);
	}

	/**
	 * Legacy fixed-closing solver retained for regression coverage.
	 * Runtime pair movement uses SolveTetheredPairVelocities above.
	 *
	 * - 保持输入共同质心速度（总平面动量守恒）；
	 * - 径向相对速度至少为 -ReelSpeed；已经更快地靠近时不反向制动；
	 * - 切向相对速度 = h/r，因此绳长缩短时角速度按 1/r² 增长；
	 * - 不写 Z，垂直运动仍由各自 CharacterMovement 负责。
	 */
	inline bool SolveRetractingPairVelocities(
		const FVector& PositionA,
		const FVector& PositionB,
		const FVector& VelocityA,
		const FVector& VelocityB,
		const double MassA,
		const double MassB,
		const double ReelSpeedCmPerSecond,
		const double SpecificAngularMomentumCm2PerSecond,
		const double MaxRelativeTangentialSpeedCmPerSecond,
		FVector& OutVelocityA,
		FVector& OutVelocityB)
	{
		OutVelocityA = FVector::ZeroVector;
		OutVelocityB = FVector::ZeroVector;
		if (!IsFiniteVector(PositionA) || !IsFiniteVector(PositionB)
			|| !IsFiniteVector(VelocityA) || !IsFiniteVector(VelocityB)
			|| !FMath::IsFinite(MassA) || !FMath::IsFinite(MassB)
			|| !FMath::IsFinite(ReelSpeedCmPerSecond)
			|| !FMath::IsFinite(SpecificAngularMomentumCm2PerSecond)
			|| !FMath::IsFinite(MaxRelativeTangentialSpeedCmPerSecond)
			|| MassA <= 0.0 || MassB <= 0.0)
		{
			return false;
		}

		const FVector Separation = FVector::VectorPlaneProject(
			PositionB - PositionA, FVector::UpVector);
		const double Distance = Separation.Size();
		if (Distance <= UE_SMALL_NUMBER)
		{
			return false;
		}

		const FVector Radial = Separation / Distance;
		const FVector Tangent(-Radial.Y, Radial.X, 0.0);
		const double SafeReelSpeed = FMath::Max(0.0, ReelSpeedCmPerSecond);
		const double SafeTangentialLimit = FMath::Max(0.0, MaxRelativeTangentialSpeedCmPerSecond);
		const FVector CurrentRelativeVelocity = FVector::VectorPlaneProject(
			VelocityB - VelocityA, FVector::UpVector);
		const double CurrentRadialSeparationSpeed = FVector::DotProduct(
			CurrentRelativeVelocity, Radial);
		const double DesiredRadialSeparationSpeed = FMath::Min(
			CurrentRadialSeparationSpeed,
			-SafeReelSpeed);
		const double RelativeTangentialSpeed = FMath::Clamp(
			SpecificAngularMomentumCm2PerSecond / Distance,
			-SafeTangentialLimit,
			SafeTangentialLimit);
		const FVector DesiredRelativeVelocity =
			Radial * DesiredRadialSeparationSpeed + Tangent * RelativeTangentialSpeed;

		const double TotalMass = MassA + MassB;
		const FVector PlanarVelocityA = FVector::VectorPlaneProject(VelocityA, FVector::UpVector);
		const FVector PlanarVelocityB = FVector::VectorPlaneProject(VelocityB, FVector::UpVector);
		const FVector CenterOfMassVelocity =
			(PlanarVelocityA * MassA + PlanarVelocityB * MassB) / TotalMass;
		OutVelocityA = CenterOfMassVelocity
			- DesiredRelativeVelocity * (MassB / TotalMass);
		OutVelocityB = CenterOfMassVelocity
			+ DesiredRelativeVelocity * (MassA / TotalMass);
		OutVelocityA.Z = VelocityA.Z;
		OutVelocityB.Z = VelocityB.Z;
		return IsFiniteVector(OutVelocityA) && IsFiniteVector(OutVelocityB);
	}
}
