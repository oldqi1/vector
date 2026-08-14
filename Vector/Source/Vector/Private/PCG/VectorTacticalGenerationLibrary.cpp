// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCG/VectorTacticalGenerationLibrary.h"

#include "PCG/VectorTacticalLayout.h"

namespace
{
	FString ModuleTypeToString(const EVectorTacticalModuleType Type)
	{
		switch (Type)
		{
		case EVectorTacticalModuleType::SafeStart: return TEXT("SafeStart");
		case EVectorTacticalModuleType::OpenBowl: return TEXT("OpenBowl");
		case EVectorTacticalModuleType::HardLane: return TEXT("HardLane");
		case EVectorTacticalModuleType::HeightShelf: return TEXT("HeightShelf");
		case EVectorTacticalModuleType::SlickCross: return TEXT("SlickCross");
		case EVectorTacticalModuleType::BossRing: return TEXT("BossRing");
		case EVectorTacticalModuleType::Extraction: return TEXT("Extraction");
		default: return TEXT("Unknown");
		}
	}
}

TArray<FString> UVectorTacticalGenerationLibrary::GenerateModuleSequence(const int32 Seed)
{
	const FVectorTacticalLayout Layout = FVectorTacticalGenerator::Generate(Seed);
	TArray<FString> Result;
	Result.Reserve(Layout.Modules.Num());
	for (const FVectorTacticalModuleDefinition& Module : Layout.Modules)
	{
		Result.Add(ModuleTypeToString(Module.Type));
	}
	return Result;
}

FString UVectorTacticalGenerationLibrary::DescribeLayout(const int32 Seed)
{
	return FVectorTacticalGenerator::Generate(Seed).Describe();
}

float UVectorTacticalGenerationLibrary::GetTacticalScore(const int32 Seed)
{
	return static_cast<float>(FVectorTacticalGenerator::Generate(Seed).TacticalScore);
}
