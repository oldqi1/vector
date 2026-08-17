// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/VectorRunProgressionTypes.h"
#include "VectorRunProgressionComponent.generated.h"

/** Current-run guaranteed calibration layer. Room clears produce a visible three-choice offer. */
UCLASS(ClassGroup = (Progression), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorRunProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorRunProgressionComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	bool SelectPendingCalibration(int32 OfferIndex);
	bool SelectPendingChoice(int32 OfferIndex);
	bool HasPendingCalibration() const { return bOfferPending; }
	bool HasPendingRuleModule() const { return bRuleModuleOfferPending; }
	bool HasPendingChoice() const { return bOfferPending || bRuleModuleOfferPending; }
	EVectorCalibrationType GetPendingCalibration(int32 OfferIndex) const;
	FString GetPendingCalibrationLabel(int32 OfferIndex) const;
	FString GetPendingChoiceLabel(int32 OfferIndex) const;
	bool HasRuleModule(EVectorRunModuleType Type) const { return SelectedRuleModule == Type; }
	EVectorRunModuleType GetSelectedRuleModule() const { return SelectedRuleModule; }
	void NotifyRuleModuleTriggered(EVectorRunModuleType Type);
	bool HasRuleModuleNotice() const { return RuleModuleNoticeSecondsRemaining > 0.0; }
	const FString& GetRuleModuleNotice() const { return RuleModuleNotice; }

	const FVectorRunCalibrationState& GetCalibrationState() const { return CalibrationState; }
	double GetRangeMultiplier() const { return CalibrationState.GetRangeMultiplier(); }
	double GetImpulseMultiplier() const { return CalibrationState.GetImpulseMultiplier(); }
	double GetRechargeIntervalMultiplier() const
	{
		return CalibrationState.GetRechargeIntervalMultiplier();
	}
	int32 GetAdditionalCells() const { return CalibrationState.GetAdditionalCells(); }
	int32 GetCompletedCalibrationCount() const { return CompletedCalibrationCount; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Progression", meta = (ClampMin = "1"))
	int32 MaximumLevelPerCalibration = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Progression", meta = (ClampMin = "1"))
	int32 MaximumRoomClearOffers = 3;

private:
	void OpenRoomClearOffer(int32 EncounterTotal);

	UPROPERTY(VisibleAnywhere, Category = "Vector|Progression")
	FVectorRunCalibrationState CalibrationState;

	TArray<EVectorCalibrationType> PendingOffer;
	TArray<EVectorRunModuleType> PendingRuleModuleOffer;
	EVectorRunModuleType SelectedRuleModule = EVectorRunModuleType::None;
	int32 CompletedCalibrationCount = 0;
	int32 LastOfferedEncounterTotal = 0;
	bool bSawActiveEnemiesSinceOffer = false;
	bool bOfferPending = false;
	bool bRuleModuleOfferPending = false;
	FString RuleModuleNotice;
	double RuleModuleNoticeSecondsRemaining = 0.0;
};
