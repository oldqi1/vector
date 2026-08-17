// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression/VectorRunProgressionComponent.h"

#include "EngineUtils.h"
#include "Hunt/VectorEncounterComponent.h"
#include "PCG/VectorPCGEncounterDirector.h"
#include "VectorGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorProgression, Log, All);

UVectorRunProgressionComponent::UVectorRunProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.10f;
}

void UVectorRunProgressionComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RuleModuleNoticeSecondsRemaining = FMath::Max(
		0.0, RuleModuleNoticeSecondsRemaining - FMath::Max(0.0f, DeltaTime));
	const AVectorGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AVectorGameMode>() : nullptr;
	const UVectorEncounterComponent* Encounter = GameMode ? GameMode->Encounter : nullptr;
	if (!Encounter || Encounter->GetEncounterState() == EVectorEncounterState::Inactive)
	{
		return;
	}
	if (Encounter->GetRemainingEnemies() > 0)
	{
		bSawActiveEnemiesSinceOffer = true;
		return;
	}
	if (!bOfferPending && !bRuleModuleOfferPending && bSawActiveEnemiesSinceOffer
		&& CompletedCalibrationCount < MaximumRoomClearOffers
		&& Encounter->GetTotalEnemies() > LastOfferedEncounterTotal)
	{
		OpenRoomClearOffer(Encounter->GetTotalEnemies());
	}
}

void UVectorRunProgressionComponent::OpenRoomClearOffer(const int32 EncounterTotal)
{
	if (FVectorRunOfferPolicy::ShouldOfferRuleModuleFirst(
		CompletedCalibrationCount, SelectedRuleModule))
	{
		PendingRuleModuleOffer = {
			EVectorRunModuleType::MomentumRecycler,
			EVectorRunModuleType::TwinVector,
			EVectorRunModuleType::LiftVectorCoupler };
		bRuleModuleOfferPending = true;
		bSawActiveEnemiesSinceOffer = false;
		LastOfferedEncounterTotal = EncounterTotal;
		UE_LOG(LogVectorProgression, Log,
			TEXT("Run first-clear rule module offered: total=%d choices=[%s|%s|%s] input=[Z|X|C] purpose=CHANGE_NEXT_ROOM_RULE"),
			EncounterTotal,
			*LexToString(PendingRuleModuleOffer[0]),
			*LexToString(PendingRuleModuleOffer[1]),
			*LexToString(PendingRuleModuleOffer[2]));
		return;
	}

	static constexpr EVectorCalibrationType OfferSets[][3] =
	{
		{ EVectorCalibrationType::Impulse, EVectorCalibrationType::Capacity,
			EVectorCalibrationType::Range },
		{ EVectorCalibrationType::Recharge, EVectorCalibrationType::Impulse,
			EVectorCalibrationType::Range },
		{ EVectorCalibrationType::Capacity, EVectorCalibrationType::Range,
			EVectorCalibrationType::Recharge },
	};
	int32 SetIndex = FMath::Clamp(
		CompletedCalibrationCount, 0, static_cast<int32>(UE_ARRAY_COUNT(OfferSets)) - 1);
	FString CircuitContext = TEXT("FIXED");
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVectorPCGEncounterDirector> It(World); It; ++It)
		{
			CircuitContext = It->GetActiveModuleName();
			if (CircuitContext == TEXT("OpenBowl"))
			{
				SetIndex = 0;
			}
			else if (CircuitContext == TEXT("HardLane"))
			{
				SetIndex = 1;
			}
			else if (CircuitContext == TEXT("HeightShelf")
				|| CircuitContext == TEXT("SlickCross"))
			{
				SetIndex = 2;
			}
			break;
		}
	}
	PendingOffer.Reset(3);
	for (const EVectorCalibrationType Type : OfferSets[SetIndex])
	{
		PendingOffer.Add(Type);
	}
	bOfferPending = true;
	bSawActiveEnemiesSinceOffer = false;
	LastOfferedEncounterTotal = EncounterTotal;
	UE_LOG(LogVectorProgression, Log,
		TEXT("Run calibration offered: clear=%d total=%d circuit=%s choices=[%s|%s|%s] input=[Z|X|C]"),
		CompletedCalibrationCount + 1, EncounterTotal, *CircuitContext,
		*LexToString(PendingOffer[0]), *LexToString(PendingOffer[1]),
		*LexToString(PendingOffer[2]));
}

bool UVectorRunProgressionComponent::SelectPendingCalibration(const int32 OfferIndex)
{
	if (!bOfferPending || !PendingOffer.IsValidIndex(OfferIndex))
	{
		return false;
	}
	const EVectorCalibrationType Type = PendingOffer[OfferIndex];
	if (!CalibrationState.Apply(Type, MaximumLevelPerCalibration))
	{
		return false;
	}
	++CompletedCalibrationCount;
	bOfferPending = false;
	PendingOffer.Reset();
	UE_LOG(LogVectorProgression, Log,
		TEXT("Run calibration installed: type=%s level=%d range=%.2f impulse=%.2f cells=+%d rechargeInterval=%.2f check=PASS"),
		*LexToString(Type), CalibrationState.GetLevel(Type),
		CalibrationState.GetRangeMultiplier(), CalibrationState.GetImpulseMultiplier(),
		CalibrationState.GetAdditionalCells(),
		CalibrationState.GetRechargeIntervalMultiplier());
	return true;
}

bool UVectorRunProgressionComponent::SelectPendingChoice(const int32 OfferIndex)
{
	if (bRuleModuleOfferPending)
	{
		if (!PendingRuleModuleOffer.IsValidIndex(OfferIndex))
		{
			return false;
		}
		SelectedRuleModule = PendingRuleModuleOffer[OfferIndex];
		bRuleModuleOfferPending = false;
		PendingRuleModuleOffer.Reset();
		RuleModuleNotice = FString::Printf(TEXT("MODULE INSTALLED: %s"),
			*LexToString(SelectedRuleModule));
		RuleModuleNoticeSecondsRemaining = 2.5;
		UE_LOG(LogVectorProgression, Log,
			TEXT("Run rule module installed: type=%s check=PASS"),
			*LexToString(SelectedRuleModule));
		return true;
	}
	return SelectPendingCalibration(OfferIndex);
}

void UVectorRunProgressionComponent::NotifyRuleModuleTriggered(
	const EVectorRunModuleType Type)
{
	if (Type == EVectorRunModuleType::None || !HasRuleModule(Type))
	{
		return;
	}
	RuleModuleNotice = FString::Printf(TEXT("MODULE TRIGGERED: %s"),
		*LexToString(Type));
	RuleModuleNoticeSecondsRemaining = 1.6;
}

EVectorCalibrationType UVectorRunProgressionComponent::GetPendingCalibration(
	const int32 OfferIndex) const
{
	return PendingOffer.IsValidIndex(OfferIndex)
		? PendingOffer[OfferIndex] : EVectorCalibrationType::Range;
}

FString UVectorRunProgressionComponent::GetPendingCalibrationLabel(
	const int32 OfferIndex) const
{
	return PendingOffer.IsValidIndex(OfferIndex)
		? LexToString(PendingOffer[OfferIndex]) : FString();
}

FString UVectorRunProgressionComponent::GetPendingChoiceLabel(
	const int32 OfferIndex) const
{
	if (bRuleModuleOfferPending)
	{
		return PendingRuleModuleOffer.IsValidIndex(OfferIndex)
			? LexToOfferDescription(PendingRuleModuleOffer[OfferIndex]) : FString();
	}
	return GetPendingCalibrationLabel(OfferIndex);
}
