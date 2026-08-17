// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/** Pure, deterministic score used to resolve overlapping cursor candidates. */
struct VECTOR_API FVectorCursorTargetScore
{
	double ScreenDistanceSquared = TNumericLimits<double>::Max();
	bool bAirborne = false;
	double SpatialDistanceSquared = TNumericLimits<double>::Max();
	FString StableName;
};

/** 镜头水平瞄准与最近物理目标裁决；所有非锤装备共用同一命中规则。 */
struct VECTOR_API FVectorCombatTargeting
{
	static FVector ComputeHorizontalAimDirection(const AActor* Owner);

	/**
	 * Projects the local mouse cursor onto the owner's horizontal plane and aims
	 * from the owner toward that point. Falls back to camera/control yaw when a
	 * cursor ray is unavailable (AI, gamepad, or a headless test).
	 */
	static FVector ComputeCursorGroundAimDirection(
		const AActor* Owner,
		bool* bOutUsedCursor = nullptr);

	/** Returns the actual WorldStatic surface under the mouse ray. */
	static bool ComputeCursorWorldStaticPoint(
		const AActor* Owner,
		FVector& OutImpactPoint,
		FVector& OutImpactNormal);
	static bool HasUnobstructedLine(const AActor* Owner, const AActor* Target);

	/** 需要目标同时具备 VectorMovement 与 Stability。 */
	static AActor* FindNearestMovableStableTarget(
		const AActor* Owner,
		const FVector& Direction,
		double RangeCm,
		double RadiusCm,
		const AActor* ExcludedActor = nullptr);

	/** Same validity rules as above, but favors the target closest to the aim ray. */
	static AActor* FindMostAlignedMovableStableTarget(
		const AActor* Owner,
		const FVector& Direction,
		double RangeCm,
		double RadiusCm,
		const AActor* ExcludedActor = nullptr);

	/**
	 * Uses the real screen cursor for selection. Candidates within the overlap
	 * tolerance favor airborne actors so lift -> vector remains intentional.
	 * Falls back to the planar aim-ray rule when no viewport cursor is available.
	 */
	static AActor* FindBestCursorMovableStableTarget(
		const AActor* Owner,
		const FVector& FallbackDirection,
		double RangeCm,
		double FallbackRadiusCm,
		double ScreenRadiusPixels,
		const AActor* ExcludedActor = nullptr);

	static bool IsCursorCandidatePreferred(
		const FVectorCursorTargetScore& Candidate,
		const FVectorCursorTargetScore& CurrentBest,
		double AirborneOverlapTolerancePixels = 18.0);

	/** 需要目标具备统一物理调质组件。 */
	static AActor* FindNearestModifierTarget(
		const AActor* Owner,
		const FVector& Direction,
		double RangeCm,
		double RadiusCm);
};
