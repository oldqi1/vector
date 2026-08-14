// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VectorTacticalGenerationLibrary.generated.h"

/** Reflection bridge used by Blueprint and Editor Python PCG tooling. */
UCLASS()
class VECTOR_API UVectorTacticalGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the validated module type sequence for a deterministic seed. */
	UFUNCTION(BlueprintPure, Category = "Vector|PCG")
	static TArray<FString> GenerateModuleSequence(int32 Seed);

	/** Human-readable generation diagnostics, including resolved seed and score. */
	UFUNCTION(BlueprintPure, Category = "Vector|PCG")
	static FString DescribeLayout(int32 Seed);

	UFUNCTION(BlueprintPure, Category = "Vector|PCG")
	static float GetTacticalScore(int32 Seed);
};
