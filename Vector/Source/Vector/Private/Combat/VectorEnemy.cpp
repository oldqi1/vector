// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemy.h"

#include "AIController.h"
#include "Combat/VectorEnemyController.h"
#include "Combat/VectorHealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEnemy, Log, All);

AVectorEnemy::AVectorEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 敌人由 AI 控制器接管（PlacedInWorldOrSpawned：编辑器放置或运行时生成都自动 Possess）。
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AVectorEnemyController::StaticClass();
}

void AVectorEnemy::BeginPlay()
{
	Super::BeginPlay();
	ApplyArchetypeConfiguration();

	// 生命归零 → 灰盒期直接销毁（正式表现：倒地/掉落，后续 Story）。
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AVectorEnemy::HandleDeath);
	}
}

void AVectorEnemy::HandleDeath()
{
	UE_LOG(LogVectorEnemy, Log, TEXT("Enemy %s died"), *GetName());
	// 先销毁 AI 控制器：UE 默认 Controller 脱离 Pawn 后残留并持续 Tick 寻路，
	// 会导致"怪杀完反而掉帧"（孤儿控制器堆积）。灰盒期直接销毁控制器。
	if (AController* EnemyController = GetController())
	{
		EnemyController->Destroy();
	}
	Destroy();
}

void AVectorEnemy::ApplyArchetypeConfiguration()
{
	// 按三型应用质量/速度/尺寸。注意同时设置基类 MassClass（ApplyMassPresentation 读它）
	// 与组件 MassClass（碰撞/稳定结算读它），保持双写一致。
	switch (Archetype)
	{
	case EVectorEnemyArchetype::LightHoppper:
		MassClass = EVectorMassClass::Light;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Light;
		}
		MoveSpeedCmPerSecond = 420.0;
		break;

	case EVectorEnemyArchetype::HeavyRhinoBeetle:
		MassClass = EVectorMassClass::Heavy;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Heavy;
		}
		MoveSpeedCmPerSecond = 180.0;
		break;

	case EVectorEnemyArchetype::ChargerRammer:
		MassClass = EVectorMassClass::Medium;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Medium;
		}
		MoveSpeedCmPerSecond = 320.0;
		break;

	default:
		break;
	}

	GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(MoveSpeedCmPerSecond);

	// 质量档变更后重新应用颜色/尺寸（Super::BeginPlay 已按默认 Medium 呈现过一次）。
	ApplyMassPresentation();
}

bool AVectorEnemy::ShouldPauseAI() const
{
	// 失衡/倒地/起身期间，或被冲量驱动（被推飞）时，AI 让路——物理状态优先。
	if (const UVectorStabilityComponent* Stability = FindComponentByClass<UVectorStabilityComponent>())
	{
		if (Stability->IsStaggered())
		{
			return true;
		}
	}
	if (const UVectorCharacterMovementComponent* Movement = FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		if (Movement->IsImpulseDriven())
		{
			return true;
		}
	}
	return false;
}
