// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorGameMode.h"

#include "VectorCharacter.h"
#include "VectorPlayerController.h"

AVectorGameMode::AVectorGameMode()
{
	DefaultPawnClass = AVectorCharacter::StaticClass();
	PlayerControllerClass = AVectorPlayerController::StaticClass();
}
