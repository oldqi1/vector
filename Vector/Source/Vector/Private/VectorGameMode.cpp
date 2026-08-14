// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorGameMode.h"

#include "Combat/VectorKillAttributionComponent.h"
#include "Hunt/VectorHuntProgressComponent.h"
#include "VectorCharacter.h"
#include "VectorHUD.h"
#include "VectorPlayerController.h"

AVectorGameMode::AVectorGameMode()
{
	DefaultPawnClass = AVectorCharacter::StaticClass();
	PlayerControllerClass = AVectorPlayerController::StaticClass();
	HUDClass = AVectorHUD::StaticClass();

	KillAttribution = CreateDefaultSubobject<UVectorKillAttributionComponent>(TEXT("KillAttribution"));
	HuntProgress = CreateDefaultSubobject<UVectorHuntProgressComponent>(TEXT("HuntProgress"));
}

void AVectorGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 局末输出击杀归因汇总（P0 验收 #6 + 死亡检查点监控）。
	if (KillAttribution)
	{
		KillAttribution->LogSummary();
	}
	Super::EndPlay(EndPlayReason);
}
