// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VectorHUD.generated.h"

/** 灰盒战斗 HUD：玩家屏幕血条 + 敌人头顶世界血条，无需额外 UMG 资产。 */
UCLASS()
class VECTOR_API AVectorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawHealthBar(
		float X,
		float Y,
		float Width,
		float Height,
		double CurrentHealth,
		double MaximumHealth,
		const FString& Label,
		bool bDrawText);
};
