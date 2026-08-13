// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VectorGameMode.generated.h"

/**
 * 冲量荒原原型默认 GameMode。
 *
 * 只负责装配默认 Pawn 与 PlayerController，不含玩法逻辑；
 * 稳定/失衡/施力等规则由专属组件在后续 Story 落地。
 */
UCLASS()
class VECTOR_API AVectorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVectorGameMode();
};
