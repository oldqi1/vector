// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VectorGameMode.generated.h"

/**
 * 冲量荒原原型默认 GameMode。
 *
 * 负责装配默认 Pawn/PlayerController，并持有击杀归因账本（P0：验收 #6 硬证据）：
 * PIE 结束（EndPlay）时输出本局击杀来源汇总与单一来源报警。
 */
UCLASS()
class VECTOR_API AVectorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVectorGameMode();

	/** 击杀归因账本（死亡检查点监控；局末 LogSummary 自动输出）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|KillAttribution")
	TObjectPtr<class UVectorKillAttributionComponent> KillAttribution;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
