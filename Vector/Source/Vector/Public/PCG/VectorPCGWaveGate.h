// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VectorPCGWaveGate.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/** Independent room gate opened by the sequential PCG encounter director. */
UCLASS()
class VECTOR_API AVectorPCGWaveGate : public AActor
{
	GENERATED_BODY()

public:
	AVectorPCGWaveGate();

	UFUNCTION(BlueprintCallable, Category = "Vector|PCG|Encounter")
	void SetGateLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Vector|PCG|Encounter")
	bool IsGateLocked() const { return bGateLocked; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Vector|PCG|Encounter")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(VisibleAnywhere, Category = "Vector|PCG|Encounter")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	UPROPERTY(VisibleAnywhere, Category = "Vector|PCG|Encounter")
	TObjectPtr<UPointLightComponent> StatusLight;

	bool bGateLocked = true;
};
