// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EVectorTacticalModuleType : uint8
{
	SafeStart,
	OpenBowl,
	HardLane,
	HeightShelf,
	SlickCross,
	BossRing,
	Extraction,
};

enum class EVectorPhysicsOpportunity : uint16
{
	None = 0,
	LongLaunchLane = 1 << 0,
	HardWallReceiver = 1 << 1,
	CrowdReceiver = 1 << 2,
	ChargeBaitLane = 1 << 3,
	TetherSwingArc = 1 << 4,
	HeightDrop = 1 << 5,
	LowFrictionPath = 1 << 6,
	RecoveryPocket = 1 << 7,
};
ENUM_CLASS_FLAGS(EVectorPhysicsOpportunity);

struct VECTOR_API FVectorTacticalModuleDefinition
{
	FName ModuleId = NAME_None;
	EVectorTacticalModuleType Type = EVectorTacticalModuleType::SafeStart;
	EVectorPhysicsOpportunity Opportunities = EVectorPhysicsOpportunity::None;
	int32 EnemyBudget = 0;
	int32 HeightLayerCount = 1;
	double MaximumHeightDifferenceCm = 0.0;

	/** Gameplay circuit contract. Geometry alone is not a valid encounter module. */
	TArray<FName> Sources;
	TArray<FName> Converters;
	TArray<FName> Receivers;
	TArray<FName> RecoveryPockets;
	TArray<FName> SupportedToolVerbs;
	TArray<FName> EnemyFunctionalSlots;
	TArray<FString> Recipes;
	FName RewardBias = NAME_None;

	int32 CountOpportunities() const;
	bool HasOpportunity(EVectorPhysicsOpportunity Opportunity) const;
	bool HasCompleteCircuit() const;
	bool HasDistinctOpenings() const;
	/** Exact '>'-delimited recipe-node queries used by deletion tests. */
	int32 CountRecipesUsingNode(FName Node) const;
	int32 CountRecipesRemainingWithoutNode(FName Node) const;
	bool HasRouteDeletionFallback(FName Node) const;
	FString DescribeCircuit() const;
};

struct VECTOR_API FVectorTacticalGenerationRules
{
	int32 EncounterModuleCount = 2;
	int32 MinimumEnemyBudget = 8;
	int32 MaximumEnemyBudget = 12;
	double MinimumTacticalScore = 12.0;
	int32 MaximumGenerationAttempts = 16;
};

struct VECTOR_API FVectorTacticalLayout
{
	int32 RequestedSeed = 0;
	int32 ResolvedSeed = 0;
	int32 GenerationAttempts = 0;
	double TacticalScore = 0.0;
	bool bValid = false;
	bool bUsedFallback = false;
	FString FailureReason;
	TArray<FVectorTacticalModuleDefinition> Modules;

	bool HasOpportunity(EVectorPhysicsOpportunity Opportunity) const;
	int32 GetMaximumHeightLayerCount() const;
	double GetMaximumHeightDifferenceCm() const;
	FString DescribeModuleSequence() const;
	FString Describe() const;
};

/** Pure deterministic PCG model. Runtime actor spawning is a later layer. */
struct VECTOR_API FVectorTacticalGenerator
{
	static FVectorTacticalLayout Generate(
		int32 Seed,
		const FVectorTacticalGenerationRules& Rules = FVectorTacticalGenerationRules());

	static bool Validate(
		const FVectorTacticalLayout& Layout,
		const FVectorTacticalGenerationRules& Rules,
		FString& OutFailureReason);

	static double ComputeTacticalScore(const FVectorTacticalLayout& Layout);
	static const TArray<FVectorTacticalModuleDefinition>& GetEncounterModuleCatalog();
};
