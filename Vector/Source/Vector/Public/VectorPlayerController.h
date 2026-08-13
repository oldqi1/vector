// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VectorPlayerController.generated.h"

/**
 * 冲量荒原默认 PlayerController。
 *
 * 原型期开启鼠标可见以便俯视角调试；不含输入捕获或瞄准规则。
 */
UCLASS()
class VECTOR_API AVectorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVectorPlayerController();

	virtual void BeginPlay() override;
};
