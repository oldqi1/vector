// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorHUD.h"

#include "Boss/VectorPhysicsBoss.h"
#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorGravityHookComponent.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorTestDummy.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Hunt/VectorContractExit.h"
#include "Hunt/VectorEncounterComponent.h"
#include "Hunt/VectorHuntProgressComponent.h"
#include "Physics/VectorPhysicsModifierComponent.h"
#include "PCG/VectorPCGEncounterDirector.h"
#include "VectorCharacter.h"
#include "VectorGameMode.h"

void AVectorHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner)
	{
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
					TEXT("1 HAMMER"),
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
			DrawText(
				TEXT("1 Hammer  2 Cable  3 Lube  4 Float  5 Lift  |  LMB Use  |  RMB Drag Camera"),
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
				FString::Printf(TEXT("SEED %d  |  WAVE %d / 3"),
					Director->GetGenerationSeed(), Director->GetActiveWaveNumber()),
				FLinearColor(0.3f, 0.85f, 1.0f),
				Canvas->ClipX - 260.0f, 36.0f,
				GEngine->GetMediumFont(), 0.9f, false);
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
			switch (Boss->GetBossPhase())
			{
			case EVectorPhysicsBossPhase::ExposedShell:
				PhaseLabel = TEXT("EXPOSED");
				break;
			case EVectorPhysicsBossPhase::Overload:
				PhaseLabel = TEXT("OVERLOAD");
				break;
			case EVectorPhysicsBossPhase::Defeated:
				PhaseLabel = TEXT("DEFEATED");
				break;
			case EVectorPhysicsBossPhase::AnchoredShell:
			default:
				break;
			}
			DrawHealthBar(
				Canvas->ClipX * 0.5f - 260.0f, 34.0f,
				520.0f, 24.0f,
				BossHealth->GetHealth(), BossHealth->GetMaxHealth(),
				FString::Printf(TEXT("MAGNET-SHELL BEAST  [%s]"), *PhaseLabel), true);
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
