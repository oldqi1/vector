// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemyAttackComponent.h"

#include "Combat/VectorTestDummy.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEnemyAttack, Log, All);

UVectorEnemyAttackComponent::UVectorEnemyAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UVectorEnemyAttackComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 物理状态优先（2026-08-14 修复）：被推飞（冲量驱动）或失衡期间不触发攻击，
	// 并中断进行中的攻击。否则怪被玩家打飞后攻击组件照常扑击，受控速度覆盖
	// 会把玩家的飞行冲量替换成反向扑击冲量——"打飞了又飞回来"的轨迹乱飞根因。
	const bool bIsImpulseDriven = Owner->FindComponentByClass<UVectorCharacterMovementComponent>()
		? Owner->FindComponentByClass<UVectorCharacterMovementComponent>()->IsImpulseDriven()
		: false;
	const bool bIsStaggered = Owner->FindComponentByClass<UVectorStabilityComponent>()
		? Owner->FindComponentByClass<UVectorStabilityComponent>()->IsStaggered()
		: false;
	// Pouncing 本身就是攻击组件发起的受控速度，不能在下一帧把自己误判为“外部冲量”取消；
	// 失衡仍始终可以中断攻击。
	const bool bExternalImpulseInterrupt = bIsImpulseDriven && AttackPhase != EAttackPhase::Pouncing;
	if (bExternalImpulseInterrupt || bIsStaggered)
	{
		if (AttackPhase != EAttackPhase::Idle)
		{
			// 中断攻击：回到 Idle，清预警表现。
			AttackPhase = EAttackPhase::Idle;
			PhaseSecondsRemaining = 0.0;
			bAttacking = false;
			UpdateWarmupPresentation(false);
		}
		return;
	}

	// 阶段计时推进（自管计时，非共享时间线——它是输入驱动语义）。
	if (PhaseSecondsRemaining > 0.0)
	{
		PhaseSecondsRemaining -= DeltaTime;
		if (PhaseSecondsRemaining <= 0.0)
		{
			switch (AttackPhase)
			{
			case EAttackPhase::Warmup:
				ExecutePounce();
				break;
			case EAttackPhase::Pouncing:
				UpdateWarmupPresentation(false);
				AttackPhase = EAttackPhase::Cooldown;
				PhaseSecondsRemaining = CooldownSeconds;
				bAttacking = true;
				break;
			case EAttackPhase::Cooldown:
				AttackPhase = EAttackPhase::Idle;
				PhaseSecondsRemaining = 0.0;
				bAttacking = false;
				break;
			default:
				break;
			}
		}
	}

	// 扑击中：实际接触、扣血与碰后速度全部由移动组件的 HandleImpact 统一结算。
	if (AttackPhase == EAttackPhase::Pouncing)
	{
		return;
	}

	if (AttackPhase != EAttackPhase::Idle)
	{
		return;
	}

	// Idle：玩家在范围内 → 触发预警（可读停顿）。
	APawn* PlayerPawn = FindPlayerPawn();
	if (!PlayerPawn)
	{
		return;
	}
	const double Distance = FVector::Dist2D(GetOwner()->GetActorLocation(), PlayerPawn->GetActorLocation());
	if (Distance <= AttackTriggerRangeCm)
	{
		BeginWarmup();
	}
}

void UVectorEnemyAttackComponent::BeginWarmup()
{
	APawn* PlayerPawn = FindPlayerPawn();
	if (!PlayerPawn)
	{
		return;
	}

	PounceDirection = (PlayerPawn->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	AttackPhase = EAttackPhase::Warmup;
	PhaseSecondsRemaining = FMath::Max(0.01, WarmupSeconds);
	bAttacking = true;
	UpdateWarmupPresentation(true);
	UE_LOG(LogVectorEnemyAttack, Log, TEXT("Enemy %s warmup (attack player)"), *GetOwner()->GetName());
}

void UVectorEnemyAttackComponent::ExecutePounce()
{
	// 预警结束：向锁定方向施加短距扑击冲量（复用受控冲量入口）。
	if (UVectorCharacterMovementComponent* Movement =
		GetOwner()->FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		Movement->QueueDirectionalVelocityOverride(PounceDirection, PounceSpeedCmPerSecond);
		UE_LOG(LogVectorEnemyAttack, Log, TEXT("Enemy %s pounce along %s"),
			*GetOwner()->GetName(), *PounceDirection.ToCompactString());
	}
	AttackPhase = EAttackPhase::Pouncing;
	PhaseSecondsRemaining = 0.25;
}

APawn* UVectorEnemyAttackComponent::FindPlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->GetPawn())
		{
			return PC->GetPawn();
		}
	}
	return nullptr;
}

void UVectorEnemyAttackComponent::UpdateWarmupPresentation(const bool bWarmup)
{
	// 灰盒可读性：预警时白色高亮（敌人"要打你了"），扑击后恢复原色。
	// 与失衡白闪共用 BodyMesh 动态材质 Color 参数（灰盒方块材质）。
	if (AVectorTestDummy* Dummy = Cast<AVectorTestDummy>(GetOwner()))
	{
		Dummy->SetAttackWarningPresentation(bWarmup);
	}
}
