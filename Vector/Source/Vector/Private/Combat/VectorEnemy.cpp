// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemy.h"

#include "AIController.h"
#include "Combat/VectorEnemyAttackComponent.h"
#include "Combat/VectorEnemyController.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Hunt/VectorOrganPickup.h"
#include "Stability/VectorStabilityComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorEnemy, Log, All);

AVectorEnemy::AVectorEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 敌人由 AI 控制器接管（PlacedInWorldOrSpawned：编辑器放置或运行时生成都自动 Possess）。
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AVectorEnemyController::StaticClass();

	// 近身攻击（P2）：进入攻击范围 → 预警 → 扑击玩家（让三种怪从"追人"变"会打人"）。
	AttackComponent = CreateDefaultSubobject<UVectorEnemyAttackComponent>(TEXT("EnemyAttack"));
	OrganPickupClass = AVectorOrganPickup::StaticClass();
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
	if (ImpactCollisionComponent)
	{
		ImpactCollisionComponent->OnBodyImpact.AddUObject(
			this, &AVectorEnemy::HandleLethalLaunchBodyImpact);
		ImpactCollisionComponent->OnSurfaceContact.AddUObject(
			this, &AVectorEnemy::HandleLethalLaunchSurfaceImpact);
	}
}

void AVectorEnemy::PrepareForHammerLethalLaunch()
{
	bLethalLaunchArmed = true;
	bLethalLaunchImpactSeen = false;
	UE_LOG(LogVectorEnemy, Log, TEXT("Lethal launch armed by hammer: enemy=%s"), *GetName());
}

void AVectorEnemy::HandleDeath()
{
	bLethalLaunchDeathActive = bLethalLaunchArmed;
	bLethalLaunchArmed = false;
	UE_LOG(LogVectorEnemy, Log, TEXT("Enemy %s died lethalLaunch=%s"),
		*GetName(), bLethalLaunchDeathActive ? TEXT("ARMED") : TEXT("no"));
	if (AttackComponent)
	{
		AttackComponent->SetComponentTickEnabled(false);
	}
	// 先销毁 AI 控制器：UE 默认 Controller 脱离 Pawn 后残留并持续 Tick 寻路，
	// 会导致"怪杀完反而掉帧"（孤儿控制器堆积）。灰盒期直接销毁控制器。
	if (AController* EnemyController = GetController())
	{
		EnemyController->Destroy();
	}
	if (bLethalLaunchDeathActive)
	{
		SetLifeSpan(static_cast<float>(FMath::Max(0.1, LethalLaunchMaximumLifetimeSeconds)));
		UE_LOG(LogVectorEnemy, Log,
			TEXT("Lethal launch active: enemy=%s maxLifetime=%.2fs"),
			*GetName(), LethalLaunchMaximumLifetimeSeconds);
		return;
	}
	SpawnOrganDrop(TEXT("normal death"));
	Destroy();
}

void AVectorEnemy::Destroyed()
{
	if (bLethalLaunchDeathActive)
	{
		SpawnOrganDrop(TEXT("lethal projectile lifetime ended"));
	}
	Super::Destroyed();
}

void AVectorEnemy::HandleLethalLaunchBodyImpact(AActor* OtherActor)
{
	if (!bLethalLaunchDeathActive || !OtherActor)
	{
		return;
	}
	ScheduleLethalLaunchDespawn(TEXT("body impact"));
}

void AVectorEnemy::HandleLethalLaunchSurfaceImpact(const double)
{
	if (!bLethalLaunchDeathActive)
	{
		return;
	}
	ScheduleLethalLaunchDespawn(TEXT("surface impact"));
}

void AVectorEnemy::ScheduleLethalLaunchDespawn(const TCHAR* Reason)
{
	if (bLethalLaunchImpactSeen)
	{
		return;
	}
	bLethalLaunchImpactSeen = true;
	SpawnOrganDrop(Reason);
	const double Delay = FMath::Max(0.01, LethalLaunchImpactDespawnDelaySeconds);
	SetLifeSpan(static_cast<float>(Delay));
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Lethal launch impact: enemy=%s reason=%s despawnIn=%.2fs"),
		*GetName(), Reason ? Reason : TEXT("unknown"), Delay);
}

void AVectorEnemy::SpawnOrganDrop(const TCHAR* Reason)
{
	if (bOrganDropSpawned || !GetWorld() || !OrganPickupClass)
	{
		return;
	}
	bOrganDropSpawned = true;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation = GetActorLocation() + FVector(0.0, 0.0, 55.0);
	AVectorOrganPickup* Pickup = GetWorld()->SpawnActor<AVectorOrganPickup>(
		OrganPickupClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Enemy organ drop: enemy=%s pickup=%s reason=%s location=%s"),
		*GetName(), *GetNameSafe(Pickup),
		Reason ? Reason : TEXT("unknown"),
		*SpawnLocation.ToCompactString());
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

	// 角槌兽使用 Controller 的专属 0.5s 预警→冲锋流程，不再同时运行普通近身扑击，
	// 避免两个攻击状态互相关闭预警表现或在近距离覆盖彼此速度。
	if (AttackComponent)
	{
		AttackComponent->SetComponentTickEnabled(Archetype != EVectorEnemyArchetype::ChargerRammer);
	}

	// 每只实例在基准速度 ± 比例范围内随机取值：同种怪速度不同，追着追着自然拉开队形，
	// 避免挤成一团（撞击效果不可见）。PVZ 普通僵尸同为"速度区间"而非固定值。
	// 用比例而非绝对值：种间基准差距（420/320/180）保留，浮动只制造种内差异。
	if (MoveSpeedVarianceRatio > 0.0)
	{
		const double Offset = MoveSpeedCmPerSecond * FMath::FRandRange(-MoveSpeedVarianceRatio, MoveSpeedVarianceRatio);
		MoveSpeedCmPerSecond = FMath::Max(20.0, MoveSpeedCmPerSecond + Offset);
		GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(MoveSpeedCmPerSecond);
	}

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
	// 攻击进行中（预警/扑击/冷却）：AI 让路，避免"一边追一边扑"。
	if (AttackComponent && AttackComponent->IsAttacking())
	{
		return true;
	}
	return false;
}

void AVectorEnemy::FellOutOfWorld(const UDamageType& DamageType)
{
	if (HealthComponent && !HealthComponent->IsDead())
	{
		UE_LOG(LogVectorEnemy, Log,
			TEXT("Enemy fell out of world: enemy=%s location=%s health=%.1f action=LETHAL_DAMAGE"),
			*GetName(), *GetActorLocation().ToCompactString(), HealthComponent->GetHealth());
		HealthComponent->ApplyDamage(FMath::Max(1.0, HealthComponent->GetHealth()));
		return;
	}
	Super::FellOutOfWorld(DamageType);
}
