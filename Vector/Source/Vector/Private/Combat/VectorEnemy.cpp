// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorEnemy.h"

#include "AIController.h"
#include "Combat/VectorBreakableAnchorComponent.h"
#include "Combat/VectorEnemyAttackComponent.h"
#include "Combat/VectorEnemyController.h"
#include "Combat/VectorEnemyRangedAttackComponent.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	RangedAttackComponent = CreateDefaultSubobject<UVectorEnemyRangedAttackComponent>(TEXT("EnemyRangedAttack"));
	BreakableAnchorComponent = CreateDefaultSubobject<UVectorBreakableAnchorComponent>(TEXT("BreakableAnchors"));

	LeftAnchorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftAnchorGroup"));
	LeftAnchorMesh->SetupAttachment(GetCapsuleComponent());
	LeftAnchorMesh->SetRelativeLocation(FVector(0.0, -92.0, -52.0));
	LeftAnchorMesh->SetRelativeScale3D(FVector(0.28, 0.42, 0.22));
	LeftAnchorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftAnchorMesh->SetGenerateOverlapEvents(false);
	LeftAnchorMesh->SetVisibility(false);

	RightAnchorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightAnchorGroup"));
	RightAnchorMesh->SetupAttachment(GetCapsuleComponent());
	RightAnchorMesh->SetRelativeLocation(FVector(0.0, 92.0, -52.0));
	RightAnchorMesh->SetRelativeScale3D(FVector(0.28, 0.42, 0.22));
	RightAnchorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightAnchorMesh->SetGenerateOverlapEvents(false);
	RightAnchorMesh->SetVisibility(false);
	if (BodyMesh && BodyMesh->GetStaticMesh())
	{
		LeftAnchorMesh->SetStaticMesh(BodyMesh->GetStaticMesh());
		RightAnchorMesh->SetStaticMesh(BodyMesh->GetStaticMesh());
	}
	LightPrototypeMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Vector/Art/PrototypeMonsters/Bat/SM_Prototype_Bat.SM_Prototype_Bat")));
	HeavyPrototypeMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Vector/Art/PrototypeMonsters/Slime/SM_Prototype_Slime.SM_Prototype_Slime")));
	ChargerPrototypeMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Vector/Art/PrototypeMonsters/Skeleton/SM_Prototype_Skeleton.SM_Prototype_Skeleton")));
	OrganPickupClass = AVectorOrganPickup::StaticClass();
}

void AVectorEnemy::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bEncounterVoidRecoveryEnabled && !bVoidRecoveryTriggered
		&& GetActorLocation().Z < EncounterVoidRecoveryFloorZ)
	{
		TriggerVoidRecovery(TEXT("encounter safety floor"));
	}
}

void AVectorEnemy::ConfigureEncounterVoidRecovery(
	const double FloorWorldZ,
	const FVector& DropRecoveryLocation)
{
	if (!FMath::IsFinite(FloorWorldZ) || DropRecoveryLocation.ContainsNaN())
	{
		return;
	}
	bEncounterVoidRecoveryEnabled = true;
	EncounterVoidRecoveryFloorZ = FloorWorldZ;
	EncounterVoidDropLocation = DropRecoveryLocation;
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Enemy void recovery configured: enemy=%s floorZ=%.0f drop=%s check=PASS"),
		*GetName(), EncounterVoidRecoveryFloorZ,
		*EncounterVoidDropLocation.ToCompactString());
}

void AVectorEnemy::BeginPlay()
{
	Super::BeginPlay();
	if (BreakableAnchorComponent)
	{
		BreakableAnchorComponent->OnAnchorGroupBroken.AddUObject(
			this, &AVectorEnemy::HandleAnchorGroupBroken);
	}
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

void AVectorEnemy::PrepareForVectorGunLethalLaunch()
{
	bLethalLaunchArmed = true;
	bLethalLaunchImpactSeen = false;
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Lethal launch armed by vector gun: enemy=%s"), *GetName());
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
	if (RangedAttackComponent)
	{
		RangedAttackComponent->SetComponentTickEnabled(false);
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
	double ArchetypeCollisionRadiusCm = 42.0;
	switch (Archetype)
	{
	case EVectorEnemyArchetype::LightHoppper:
		MassClass = EVectorMassClass::Light;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Light;
		}
		MoveSpeedCmPerSecond = 420.0;
		ArchetypeCollisionRadiusCm = 34.0;
		break;

	case EVectorEnemyArchetype::HeavyRhinoBeetle:
		MassClass = EVectorMassClass::Heavy;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Heavy;
		}
		MoveSpeedCmPerSecond = 180.0;
		ArchetypeCollisionRadiusCm = 72.0;
		break;

	case EVectorEnemyArchetype::ChargerRammer:
		MassClass = EVectorMassClass::Medium;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Medium;
		}
		MoveSpeedCmPerSecond = 320.0;
		ArchetypeCollisionRadiusCm = 44.0;
		break;

	case EVectorEnemyArchetype::ArcShell:
		MassClass = EVectorMassClass::Medium;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Medium;
		}
		MoveSpeedCmPerSecond = 230.0;
		ArchetypeCollisionRadiusCm = 54.0;
		break;

	case EVectorEnemyArchetype::CorrosionDrone:
		MassClass = EVectorMassClass::Light;
		if (StabilityComponent)
		{
			StabilityComponent->MassClass = EVectorMassClass::Light;
		}
		MoveSpeedCmPerSecond = 300.0;
		ArchetypeCollisionRadiusCm = 32.0;
		break;

	default:
		break;
	}

	GetCapsuleComponent()->SetCapsuleRadius(
		static_cast<float>(ArchetypeCollisionRadiusCm), true);
	GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(MoveSpeedCmPerSecond);
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Enemy navigation footprint: enemy=%s archetype=%s radius=%.0f speed=%.0f check=PASS"),
		*GetName(), *UEnum::GetValueAsString(Archetype),
		ArchetypeCollisionRadiusCm, MoveSpeedCmPerSecond);

	// 角槌兽使用 Controller 的专属 0.5s 预警→冲锋流程，不再同时运行普通近身扑击，
	// 避免两个攻击状态互相关闭预警表现或在近距离覆盖彼此速度。
	if (AttackComponent)
	{
		const bool bUsesStandardMelee = Archetype != EVectorEnemyArchetype::ChargerRammer
			&& Archetype != EVectorEnemyArchetype::ArcShell
			&& Archetype != EVectorEnemyArchetype::CorrosionDrone;
		AttackComponent->SetComponentTickEnabled(bUsesStandardMelee);
	}
	if (RangedAttackComponent)
	{
		const EVectorEnemyRangedPattern RangedPattern =
			Archetype == EVectorEnemyArchetype::ArcShell
				? EVectorEnemyRangedPattern::ArcWeakHoming
				: Archetype == EVectorEnemyArchetype::CorrosionDrone
					? EVectorEnemyRangedPattern::CorrosionVolley
					: EVectorEnemyRangedPattern::None;
		RangedAttackComponent->ConfigurePattern(RangedPattern);
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
	ApplyPrototypeMeshPresentation();
	if (BreakableAnchorComponent)
	{
		BreakableAnchorComponent->SetStructureEnabled(
			Archetype == EVectorEnemyArchetype::HeavyRhinoBeetle);
	}
	UpdateAnchorPresentation();
}

void AVectorEnemy::ApplyPrototypeMeshPresentation()
{
	if (!BodyMesh)
	{
		return;
	}

	TSoftObjectPtr<UStaticMesh> SelectedMesh = PrototypeMeshOverride;
	FVector SelectedScale = PrototypeMeshScaleOverride;
	if (SelectedMesh.IsNull())
	{
		switch (Archetype)
		{
		case EVectorEnemyArchetype::LightHoppper:
			SelectedMesh = LightPrototypeMesh;
			SelectedScale = FVector(0.28);
			break;
		case EVectorEnemyArchetype::HeavyRhinoBeetle:
			SelectedMesh = HeavyPrototypeMesh;
			SelectedScale = FVector(0.72);
			break;
		case EVectorEnemyArchetype::ChargerRammer:
			SelectedMesh = ChargerPrototypeMesh;
			SelectedScale = FVector(0.34);
			break;
		case EVectorEnemyArchetype::ArcShell:
			SelectedMesh = HeavyPrototypeMesh;
			SelectedScale = FVector(0.48);
			break;
		case EVectorEnemyArchetype::CorrosionDrone:
			SelectedMesh = LightPrototypeMesh;
			SelectedScale = FVector(0.26);
			break;
		default:
			break;
		}
	}

	UStaticMesh* LoadedMesh = SelectedMesh.IsNull() ? nullptr : SelectedMesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		UE_LOG(LogVectorEnemy, Verbose,
			TEXT("Prototype creature mesh unavailable; greybox retained: enemy=%s asset=%s"),
			*GetName(), *SelectedMesh.ToSoftObjectPath().ToString());
		return;
	}

	if (SelectedScale.IsNearlyZero())
	{
		SelectedScale = FVector(1.0);
	}
	SelectedScale.X = FMath::Max(0.01, FMath::Abs(SelectedScale.X));
	SelectedScale.Y = FMath::Max(0.01, FMath::Abs(SelectedScale.Y));
	SelectedScale.Z = FMath::Max(0.01, FMath::Abs(SelectedScale.Z));
	BodyMesh->SetStaticMesh(LoadedMesh);
	BodyMesh->EmptyOverrideMaterials();
	BaseBodyScale = SelectedScale;
	// Quaternius OBJ source uses Y-up. Roll +90 degrees maps its authored
	// vertical axis to Unreal's Z-up without changing gameplay collision.
	BaseBodyRotation = FRotator(0.0, 0.0, 90.0);
	BodyMesh->SetRelativeScale3D(BaseBodyScale);
	BodyMesh->SetRelativeRotation(BaseBodyRotation);

	// Place the corrected mesh's actual lower bound on the capsule floor. Bounds
	// must be rotated first or Y-up assets appear buried/floating after standing up.
	const FBoxSphereBounds Bounds = LoadedMesh->GetBounds().TransformBy(
		FTransform(BaseBodyRotation));
	const double BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	BodyMesh->SetRelativeLocation(FVector(
		-Bounds.Origin.X * SelectedScale.X,
		-Bounds.Origin.Y * SelectedScale.Y,
		-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - BottomZ * SelectedScale.Z));
	BodyMaterial = nullptr;
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Prototype creature mesh applied: enemy=%s archetype=%s mesh=%s scale=%s rotation=%s"),
		*GetName(), *UEnum::GetValueAsString(Archetype), *LoadedMesh->GetName(),
		*SelectedScale.ToCompactString(), *BaseBodyRotation.ToCompactString());
}

void AVectorEnemy::HandleAnchorGroupBroken(
	const EVectorAnchorGroupSide Side,
	const int32 BrokenGroupCount)
{
	UpdateAnchorPresentation();
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Enemy anchor presentation: enemy=%s side=%s broken=%d/2 launchable=%s"),
		*GetName(),
		Side == EVectorAnchorGroupSide::Right ? TEXT("RIGHT") : TEXT("LEFT"),
		BrokenGroupCount,
		BreakableAnchorComponent && BreakableAnchorComponent->IsLaunchable()
			? TEXT("YES") : TEXT("no"));
}

void AVectorEnemy::UpdateAnchorPresentation()
{
	const bool bHeavy = Archetype == EVectorEnemyArchetype::HeavyRhinoBeetle;
	if (LeftAnchorMesh)
	{
		LeftAnchorMesh->SetVisibility(bHeavy && BreakableAnchorComponent
			&& !BreakableAnchorComponent->IsGroupBroken(EVectorAnchorGroupSide::Left));
	}
	if (RightAnchorMesh)
	{
		RightAnchorMesh->SetVisibility(bHeavy && BreakableAnchorComponent
			&& !BreakableAnchorComponent->IsGroupBroken(EVectorAnchorGroupSide::Right));
	}
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
	if (RangedAttackComponent && RangedAttackComponent->IsCommittingAttack())
	{
		return true;
	}
	return false;
}

void AVectorEnemy::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!bVoidRecoveryTriggered)
	{
		TriggerVoidRecovery(TEXT("world KillZ"));
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

void AVectorEnemy::TriggerVoidRecovery(const TCHAR* Reason)
{
	if (bVoidRecoveryTriggered)
	{
		return;
	}
	bVoidRecoveryTriggered = true;
	const FVector FallenLocation = GetActorLocation();
	UE_LOG(LogVectorEnemy, Log,
		TEXT("Enemy void recovery: enemy=%s reason=%s fallen=%s floorZ=%.0f ledgerAction=LETHAL_DAMAGE"),
		*GetName(), Reason ? Reason : TEXT("unknown"),
		*FallenLocation.ToCompactString(), EncounterVoidRecoveryFloorZ);

	if (HealthComponent && !HealthComponent->IsDead())
	{
		// Death and its organ drop occur at the authored recovery point so a
		// successful environmental kill cannot strand progression or loot below the map.
		if (bEncounterVoidRecoveryEnabled)
		{
			SetActorLocation(EncounterVoidDropLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		HealthComponent->ApplyDamage(FMath::Max(1.0, HealthComponent->GetHealth()));
		return;
	}
	Destroy();
}
