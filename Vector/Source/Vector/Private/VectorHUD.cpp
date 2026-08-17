// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorHUD.h"

#include "Boss/VectorPhysicsBoss.h"
#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorGravityHookComponent.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorGunComponent.h"
#include "Combat/VectorTestDummy.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Hunt/VectorContractExit.h"
#include "Hunt/VectorEncounterComponent.h"
#include "Hunt/VectorHuntProgressComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/VectorPhysicsModifierComponent.h"
#include "PCG/VectorPCGEncounterDirector.h"
#include "Progression/VectorRunProgressionComponent.h"
#include "VectorCharacter.h"
#include "VectorGameMode.h"

void AVectorHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner)
	{
		return;
	}
	if (UGameplayStatics::IsGamePaused(this) && GEngine)
	{
		const float PanelWidth = 520.0f;
		const float PanelHeight = 150.0f;
		const float PanelX = (Canvas->ClipX - PanelWidth) * 0.5f;
		const float PanelY = (Canvas->ClipY - PanelHeight) * 0.5f;
		DrawRect(FLinearColor(0.01f, 0.02f, 0.035f, 0.94f),
			PanelX, PanelY, PanelWidth, PanelHeight);
		DrawText(TEXT("PAUSED"), FLinearColor(0.1f, 0.95f, 1.0f),
			PanelX + 195.0f, PanelY + 28.0f,
			GEngine->GetLargeFont(), 1.0f, false);
		DrawText(TEXT("ESC  RESUME"), FLinearColor::White,
			PanelX + 118.0f, PanelY + 88.0f,
			GEngine->GetMediumFont(), 0.8f, false);
		DrawText(TEXT("Q  QUIT"), FLinearColor(1.0f, 0.3f, 0.2f),
			PanelX + 310.0f, PanelY + 88.0f,
			GEngine->GetMediumFont(), 0.8f, false);
		return;
	}

	if (APawn* PlayerPawn = PlayerOwner->GetPawn())
	{
		if (GEngine)
		{
			const AVectorGameMode* GameMode = GetWorld()
				? GetWorld()->GetAuthGameMode<AVectorGameMode>() : nullptr;
			if (const UVectorHuntProgressComponent* Hunt = GameMode
				? GameMode->HuntProgress : nullptr)
			{
				DrawText(
					FString::Printf(TEXT("ORGANS: %d"), Hunt->GetCollectedOrgans()),
					FLinearColor(0.25f, 1.0f, 0.4f),
					40.0f, 36.0f,
					GEngine->GetMediumFont(), 1.0f, false);
				if (Hunt->IsExtractionComplete())
				{
					DrawText(
						FString::Printf(TEXT("HUNT COMPLETE - ORGANS SECURED: %d"),
							Hunt->GetSecuredOrgans()),
						FLinearColor(0.15f, 1.0f, 0.35f),
						Canvas->ClipX * 0.5f - 230.0f,
						Canvas->ClipY * 0.28f,
						GEngine->GetLargeFont(), 1.0f, false);
				}
			}
			if (const UVectorEncounterComponent* Encounter = GameMode
				? GameMode->Encounter : nullptr)
			{
				FString ContractText;
				FLinearColor ContractColor(1.0f, 0.72f, 0.12f);
				if (Encounter->IsComplete())
				{
					ContractText = TEXT("CONTRACT COMPLETE - EXIT OPEN");
					ContractColor = FLinearColor(0.1f, 1.0f, 0.25f);
				}
				else if (Encounter->GetEncounterState() == EVectorEncounterState::Active)
				{
					if (Encounter->IsDynamicEncounter()
						&& !Encounter->IsEncounterSealed()
						&& Encounter->GetRemainingEnemies() == 0)
					{
						ContractText = Encounter->GetTotalEnemies() == 0
							? TEXT("CONTRACT: ENTER THE HUNT")
							: TEXT("ROOM CLEAR - ADVANCE");
						ContractColor = FLinearColor(0.25f, 0.9f, 1.0f);
					}
					else
					{
						ContractText = FString::Printf(TEXT("CONTRACT: HUNT  %d / %d"),
							Encounter->GetRemainingEnemies(), Encounter->GetTotalEnemies());
					}
				}
				if (!ContractText.IsEmpty())
				{
					DrawText(ContractText, ContractColor, 40.0f, 62.0f,
						GEngine->GetMediumFont(), 0.9f, false);
				}
			}
		}
		if (const UVectorHealthComponent* Health =
			PlayerPawn->FindComponentByClass<UVectorHealthComponent>())
		{
			DrawHealthBar(
				40.0f,
				Canvas->ClipY - 72.0f,
				360.0f,
				26.0f,
				Health->GetHealth(),
				Health->GetMaxHealth(),
				TEXT("PLAYER"),
				true);
		}
		if (GEngine)
		{
			if (const AVectorCharacter* VectorCharacter = Cast<AVectorCharacter>(PlayerPawn))
			{
				static const TCHAR* EquipmentLabels[] =
				{
					TEXT("1 VECTOR"),
					TEXT("2 CABLE"),
					TEXT("3 LUBE"),
					TEXT("4 FLOAT"),
					TEXT("5 LIFT"),
				};
				constexpr float SlotWidth = 72.0f;
				constexpr float SlotHeight = 30.0f;
				constexpr float SlotGap = 5.0f;
				constexpr int32 SlotCount = UE_ARRAY_COUNT(EquipmentLabels);
				const float BarWidth = SlotCount * SlotWidth + (SlotCount - 1) * SlotGap;
				const float BarX = (Canvas->ClipX - BarWidth) * 0.5f;
				const float BarY = Canvas->ClipY - 70.0f;
				const int32 SelectedIndex = static_cast<int32>(
					VectorCharacter->GetSelectedEquipmentSlot());
				for (int32 Index = 0; Index < SlotCount; ++Index)
				{
					const float SlotX = BarX + Index * (SlotWidth + SlotGap);
					const bool bSelected = Index == SelectedIndex;
					DrawRect(
						bSelected ? FLinearColor(0.1f, 0.95f, 1.0f, 0.95f)
							: FLinearColor(0.15f, 0.18f, 0.22f, 0.8f),
						SlotX - 2.0f, BarY - 2.0f, SlotWidth + 4.0f, SlotHeight + 4.0f);
					DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.9f),
						SlotX, BarY, SlotWidth, SlotHeight);
					DrawText(EquipmentLabels[Index],
						bSelected ? FLinearColor::White : FLinearColor(0.65f, 0.7f, 0.78f),
						SlotX + 7.0f, BarY + 8.0f,
						GEngine->GetSmallFont(), 0.65f, false);
				}
			}
			const AVectorCharacter* EquipmentCharacter =
				Cast<AVectorCharacter>(PlayerPawn);
			const UVectorRunProgressionComponent* HintProgression =
				PlayerPawn->FindComponentByClass<UVectorRunProgressionComponent>();
			const bool bSlamModuleInstalled = HintProgression
				&& HintProgression->HasRuleModule(
					EVectorRunModuleType::LiftVectorCoupler);
			const FString ControlHint = EquipmentCharacter
				&& EquipmentCharacter->GetSelectedEquipmentSlot()
					== EVectorEquipmentSlot::LiftFork
				? (bSlamModuleInstalled
					? TEXT("5 LIFT + COUPLER  |  TAP: LOW SHOCK  |  HOLD: AUTO SLAM  |  DRAG: MANUAL")
					: TEXT("5 LIFT  |  LMB: LIFT + LOW SHOCK  |  FOLLOW WITH 1 VECTOR GUN"))
				: TEXT("1 Vector Gun  2 Cable  3 Lube  4 Float  5 Lift  |  LMB Use  |  RMB Drag Camera");
			DrawText(
				ControlHint,
				FLinearColor(0.85f, 0.9f, 1.0f),
				40.0f,
				Canvas->ClipY - 102.0f,
				GEngine->GetSmallFont(),
				0.85f,
				false);
			if (const AVectorCharacter* VectorCharacter = Cast<AVectorCharacter>(PlayerPawn))
			{
				DrawText(
					FString::Printf(TEXT("EQUIP: %s"), *VectorCharacter->GetSelectedEquipmentLabel()),
					FLinearColor(0.25f, 0.95f, 1.0f),
					40.0f,
					Canvas->ClipY - 124.0f,
					GEngine->GetSmallFont(),
					0.85f,
					false);
			}
			if (const UVectorGunComponent* Gun =
				PlayerPawn->FindComponentByClass<UVectorGunComponent>())
			{
				DrawText(
					FString::Printf(TEXT("VECTOR CELLS: %d/%d  RANGE %.0f  IMPULSE %.0f"),
						Gun->GetCurrentCells(), Gun->GetMaximumCells(),
						Gun->GetEffectiveRangeCm(), Gun->GetEffectiveImpulseBudget()),
					FLinearColor(0.15f, 0.95f, 1.0f),
					40.0f, Canvas->ClipY - 168.0f,
					GEngine->GetSmallFont(), 0.82f, false);
			}
			if (const UVectorRunProgressionComponent* Progression =
				PlayerPawn->FindComponentByClass<UVectorRunProgressionComponent>())
			{
				const FVectorRunCalibrationState& State = Progression->GetCalibrationState();
				DrawText(
					FString::Printf(TEXT("CALIBRATION  RANGE L%d  IMPULSE L%d  BATTERY L%d  RECHARGE L%d  MODULE %s"),
						State.RangeLevel, State.ImpulseLevel,
						State.CapacityLevel, State.RechargeLevel,
						*LexToString(Progression->GetSelectedRuleModule())),
					FLinearColor(0.8f, 0.85f, 0.95f),
					40.0f, Canvas->ClipY - 190.0f,
					GEngine->GetSmallFont(), 0.75f, false);
				if (Progression->HasPendingChoice())
				{
					const float OfferWidth = 900.0f;
					const float OfferX = (Canvas->ClipX - OfferWidth) * 0.5f;
					const float OfferY = Canvas->ClipY * 0.22f;
					const bool bRuleModuleOffer = Progression->HasPendingRuleModule();
					const float OfferHeight = bRuleModuleOffer ? 168.0f : 92.0f;
					DrawRect(FLinearColor(0.01f, 0.025f, 0.04f, 0.92f),
						OfferX, OfferY, OfferWidth, OfferHeight);
					DrawText(
						bRuleModuleOffer
							? TEXT("SYSTEM UPGRADE - INSTALL ONE RULE MODULE")
							: TEXT("ROOM CLEAR - INSTALL ONE BASE CALIBRATION"),
						FLinearColor(0.15f, 0.95f, 1.0f), OfferX + 210.0f, OfferY + 12.0f,
						GEngine->GetMediumFont(), 0.9f, false);
					if (bRuleModuleOffer)
					{
						static const TCHAR* ChoiceKeys[] = { TEXT("Z"), TEXT("X"), TEXT("C") };
						for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
						{
							DrawText(
								FString::Printf(TEXT("[%s]  %s"), ChoiceKeys[ChoiceIndex],
									*Progression->GetPendingChoiceLabel(ChoiceIndex)),
								FLinearColor::White, OfferX + 70.0f,
								OfferY + 54.0f + ChoiceIndex * 32.0f,
								GEngine->GetSmallFont(), 0.82f, false);
						}
					}
					else
					{
						DrawText(
							FString::Printf(TEXT("[Z] %s     [X] %s     [C] %s"),
								*Progression->GetPendingChoiceLabel(0),
								*Progression->GetPendingChoiceLabel(1),
								*Progression->GetPendingChoiceLabel(2)),
							FLinearColor::White, OfferX + 72.0f, OfferY + 54.0f,
							GEngine->GetSmallFont(), 0.9f, false);
					}
				}
				if (Progression->HasRuleModuleNotice())
				{
					const float NoticeWidth = 520.0f;
					const float NoticeX = (Canvas->ClipX - NoticeWidth) * 0.5f;
					const float NoticeY = Canvas->ClipY * 0.14f;
					DrawRect(FLinearColor(0.02f, 0.06f, 0.08f, 0.86f),
						NoticeX, NoticeY, NoticeWidth, 42.0f);
					DrawText(Progression->GetRuleModuleNotice(),
						FLinearColor(0.15f, 0.95f, 1.0f),
						NoticeX + 55.0f, NoticeY + 12.0f,
						GEngine->GetSmallFont(), 0.82f, false);
				}
			}
			if (const UVectorActionLockComponent* Lock =
				PlayerPawn->FindComponentByClass<UVectorActionLockComponent>(); Lock && Lock->IsLocked())
			{
				DrawText(
					FString::Printf(TEXT("ACTION: %s"), *Lock->GetActiveActionName().ToString()),
					FLinearColor::Yellow,
					40.0f,
					Canvas->ClipY - 146.0f,
					GEngine->GetSmallFont(),
					0.85f,
					false);
			}
			if (const UVectorGravityHookComponent* Cable =
				PlayerPawn->FindComponentByClass<UVectorGravityHookComponent>())
			{
				FString CableStatus;
				if (Cable->IsHookActive())
				{
					switch (Cable->GetHookMode())
					{
					case EVectorGravityHookMode::AwaitingSecondEndpoint:
						CableStatus = TEXT("CABLE: SELECT SECOND MONSTER");
						break;
					case EVectorGravityHookMode::PullingPlayerToAnchor:
						CableStatus = TEXT("CABLE: WALL REEL");
						break;
					case EVectorGravityHookMode::RetractingPair:
						if (Cable->GetPairSwingSecondsRemaining() > 0.0)
						{
							CableStatus = FString::Printf(TEXT("CABLE: WIDE SWING  %.1fs"),
								Cable->GetPairSwingSecondsRemaining());
						}
						else if (Cable->GetPairSetupSecondsRemaining() > 0.0)
						{
							CableStatus = FString::Printf(TEXT("CABLE: HAMMER NOW  %.1fs"),
								Cable->GetPairSetupSecondsRemaining());
						}
						else
						{
							CableStatus = TEXT("CABLE: PAIR WINCHING");
						}
						break;
					default:
						break;
					}
				}
				else if (Cable->IsOnCooldown())
				{
					CableStatus = FString::Printf(
						TEXT("CABLE CD: %.1fs"), Cable->GetCooldownSecondsRemaining());
				}
				if (!CableStatus.IsEmpty())
				{
					DrawText(CableStatus, FLinearColor(0.95f, 0.25f, 0.95f),
						40.0f, Canvas->ClipY - 168.0f,
						GEngine->GetSmallFont(), 0.85f, false);
				}
			}
			if (const UVectorPhysicsModifierComponent* Modifier =
				PlayerPawn->FindComponentByClass<UVectorPhysicsModifierComponent>();
				Modifier && Modifier->IsEnvironmentFrictionModified())
			{
				DrawText(
					TEXT("SURFACE: LOW FRICTION"),
					FLinearColor(0.15f, 0.9f, 1.0f),
					40.0f,
					Canvas->ClipY - 190.0f,
					GEngine->GetSmallFont(),
					0.85f,
					false);
			}
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (GEngine)
	{
		for (TActorIterator<AVectorPCGEncounterDirector> It(World); It; ++It)
		{
			const AVectorPCGEncounterDirector* Director = *It;
			if (!Director)
			{
				continue;
			}
			DrawText(
				FString::Printf(TEXT("SEED %d  |  WAVE %d / 3  |  %s"),
					Director->GetGenerationSeed(), Director->GetActiveWaveNumber(),
					*Director->GetActiveModuleName()),
				FLinearColor(0.3f, 0.85f, 1.0f),
				Canvas->ClipX - 390.0f, 36.0f,
				GEngine->GetMediumFont(), 0.9f, false);
			const float LegendX = Canvas->ClipX - 390.0f;
			const float LegendY = 68.0f;
			DrawRect(FLinearColor(0.01f, 0.025f, 0.04f, 0.78f),
				LegendX - 12.0f, LegendY - 8.0f, 355.0f, 58.0f);
			DrawText(TEXT("CIRCUIT"), FLinearColor::White,
				LegendX, LegendY, GEngine->GetSmallFont(), 0.72f, false);
			DrawText(TEXT("SOURCE"), FLinearColor(1.0f, 0.28f, 0.03f),
				LegendX + 70.0f, LegendY, GEngine->GetSmallFont(), 0.72f, false);
			DrawText(TEXT(">"), FLinearColor(0.75f, 0.8f, 0.85f),
				LegendX + 129.0f, LegendY, GEngine->GetSmallFont(), 0.72f, false);
			DrawText(TEXT("CONVERTER"), FLinearColor(0.02f, 0.9f, 1.0f),
				LegendX + 143.0f, LegendY, GEngine->GetSmallFont(), 0.72f, false);
			DrawText(TEXT(">"), FLinearColor(0.75f, 0.8f, 0.85f),
				LegendX + 224.0f, LegendY, GEngine->GetSmallFont(), 0.72f, false);
			DrawText(TEXT("RECEIVER"), FLinearColor(1.0f, 0.06f, 0.62f),
				LegendX + 238.0f, LegendY, GEngine->GetSmallFont(), 0.72f, false);
			const bool bInSafeStart = Director->GetActiveWaveNumber() <= 0;
			DrawText(
				bInSafeStart
					? TEXT("READ THE MACHINE, THEN ADVANCE EAST")
					: TEXT("BUILD SPEED, CHANGE PATH, CASH OUT THE HIT"),
				FLinearColor(0.78f, 0.84f, 0.9f),
				LegendX, LegendY + 24.0f,
				GEngine->GetSmallFont(), 0.65f, false);
			if (bInSafeStart)
			{
				const float CardWidth = 760.0f;
				const float CardX = (Canvas->ClipX - CardWidth) * 0.5f;
				const float CardY = Canvas->ClipY * 0.16f;
				DrawRect(FLinearColor(0.01f, 0.025f, 0.04f, 0.84f),
					CardX, CardY, CardWidth, 76.0f);
				DrawText(TEXT("TOOL SENTENCE"), FLinearColor(0.15f, 0.95f, 1.0f),
					CardX + 300.0f, CardY + 12.0f,
					GEngine->GetMediumFont(), 0.82f, false);
				DrawText(
					TEXT("[1] INJECT VECTOR   [2] GUIDE   [3/4] CHANGE RULES   [5] CHANGE PLANE"),
					FLinearColor::White, CardX + 55.0f, CardY + 46.0f,
					GEngine->GetSmallFont(), 0.76f, false);
			}
			break;
		}

		for (TActorIterator<AVectorPhysicsBoss> It(World); It; ++It)
		{
			const AVectorPhysicsBoss* Boss = *It;
			const UVectorHealthComponent* BossHealth = Boss
				? Boss->FindComponentByClass<UVectorHealthComponent>() : nullptr;
			if (!Boss || !BossHealth || BossHealth->IsDead())
			{
				continue;
			}

			FString PhaseLabel = TEXT("ANCHORED");
			FString BossObjective = TEXT("BREAK SHELL: VECTOR CYAN ORB INTO EITHER SIDE");
			switch (Boss->GetBossPhase())
			{
			case EVectorPhysicsBossPhase::ExposedShell:
				PhaseLabel = TEXT("EXPOSED");
				BossObjective = TEXT("ONE ANCHOR LEFT: STRIKE THE OTHER SIDE");
				break;
			case EVectorPhysicsBossPhase::Overload:
				PhaseLabel = TEXT("OVERLOAD");
				BossObjective = TEXT("CORE OPEN: BUILD SPEED AND COLLIDE");
				break;
			case EVectorPhysicsBossPhase::Defeated:
				PhaseLabel = TEXT("DEFEATED");
				BossObjective = TEXT("PHYSICS CIRCUIT COMPLETE");
				break;
			case EVectorPhysicsBossPhase::AnchoredShell:
			default:
				break;
			}
			if (Boss->IsStaggerResolveActive())
			{
				PhaseLabel += TEXT(" | RESOLVE");
			}
			DrawHealthBar(
				Canvas->ClipX * 0.5f - 260.0f, 34.0f,
				520.0f, 24.0f,
				BossHealth->GetHealth(), BossHealth->GetMaxHealth(),
				FString::Printf(TEXT("MAGNET-SHELL BEAST  [%s]"), *PhaseLabel), true);
			DrawText(BossObjective, FLinearColor(0.2f, 0.9f, 1.0f),
				Canvas->ClipX * 0.5f - 210.0f, 64.0f,
				GEngine->GetSmallFont(), 0.78f, false);
			break;
		}
	}

	if (GEngine)
	{
		for (TActorIterator<AVectorContractExit> It(World); It; ++It)
		{
			const AVectorContractExit* Exit = *It;
			FVector2D ScreenPosition;
			if (!Exit || !PlayerOwner->ProjectWorldLocationToScreen(
				Exit->GetActorLocation() + FVector(0.0, 0.0, 280.0),
				ScreenPosition, true))
			{
				continue;
			}
			const bool bOpen = Exit->IsUnlocked();
			DrawText(
				bOpen ? TEXT("EXIT OPEN") : TEXT("EXIT LOCKED - CLEAR ALL"),
				bOpen ? FLinearColor(0.1f, 1.0f, 0.25f) : FLinearColor(1.0f, 0.12f, 0.04f),
				ScreenPosition.X - (bOpen ? 48.0f : 100.0f),
				ScreenPosition.Y,
				GEngine->GetMediumFont(),
				0.85f,
				false);
		}
	}

	// VectorEnemy derives from VectorTestDummy. Iterating the shared base keeps
	// health/modifier feedback visible on both live enemies and inert test rigs.
	for (TActorIterator<AVectorTestDummy> It(World); It; ++It)
	{
		const AVectorTestDummy* Target = *It;
		const UVectorHealthComponent* Health = Target
			? Target->FindComponentByClass<UVectorHealthComponent>()
			: nullptr;
		if (!Target || !Health || Health->IsDead())
		{
			continue;
		}

		FVector BoundsOrigin;
		FVector BoundsExtent;
		Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);
		FVector2D ScreenPosition;
		const FVector BarWorldLocation = BoundsOrigin + FVector(0.0, 0.0, BoundsExtent.Z + 45.0);
		if (!PlayerOwner->ProjectWorldLocationToScreen(BarWorldLocation, ScreenPosition, true))
		{
			continue;
		}

		constexpr float BarWidth = 82.0f;
		constexpr float BarHeight = 8.0f;
		DrawHealthBar(
			ScreenPosition.X - BarWidth * 0.5f,
			ScreenPosition.Y,
			BarWidth,
			BarHeight,
			Health->GetHealth(),
			Health->GetMaxHealth(),
			FString(),
			false);

		// 调质器状态徽标：无需额外 UMG 资产，蓝=LUBE、青=FLOAT，并显示剩余秒数。
		if (const UVectorPhysicsModifierComponent* Modifier =
			GEngine ? Target->FindComponentByClass<UVectorPhysicsModifierComponent>() : nullptr)
		{
			float BadgeX = ScreenPosition.X - BarWidth * 0.5f;
			const float BadgeY = ScreenPosition.Y + BarHeight + 5.0f;
			if (Modifier->IsLubricated())
			{
				const FString Text = FString::Printf(TEXT("LUBE %.1f"), Modifier->GetLubricantSecondsRemaining());
				DrawText(Text, FLinearColor(0.2f, 0.45f, 1.0f), BadgeX, BadgeY,
					GEngine->GetSmallFont(), 0.65f, false);
				BadgeX += 54.0f;
			}
			if (Modifier->IsBuoyant())
			{
				const FString Text = FString::Printf(TEXT("FLOAT %.1f"), Modifier->GetBuoyantSecondsRemaining());
				DrawText(Text, FLinearColor(0.2f, 0.95f, 1.0f), BadgeX, BadgeY,
					GEngine->GetSmallFont(), 0.65f, false);
			}
		}
	}
}

void AVectorHUD::DrawHealthBar(
	const float X,
	const float Y,
	const float Width,
	const float Height,
	const double CurrentHealth,
	const double MaximumHealth,
	const FString& Label,
	const bool bDrawText)
{
	const double SafeMaximum = FMath::Max(1.0, MaximumHealth);
	const float Ratio = static_cast<float>(FMath::Clamp(CurrentHealth / SafeMaximum, 0.0, 1.0));
	const FLinearColor FillColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.85f, 0.08f, 0.05f, 1.0f),
		FLinearColor(0.05f, 0.8f, 0.15f, 1.0f),
		Ratio);

	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.90f), X - 2.0f, Y - 2.0f, Width + 4.0f, Height + 4.0f);
	DrawRect(FLinearColor(0.18f, 0.03f, 0.03f, 0.95f), X, Y, Width, Height);
	DrawRect(FillColor, X, Y, Width * Ratio, Height);

	if (bDrawText && GEngine)
	{
		const FString Text = FString::Printf(
			TEXT("%s  %.0f / %.0f"),
			*Label,
			FMath::Max(0.0, CurrentHealth),
			SafeMaximum);
		DrawText(Text, FLinearColor::White, X + 8.0f, Y + 3.0f, GEngine->GetSmallFont(), 1.0f, false);
	}
}
