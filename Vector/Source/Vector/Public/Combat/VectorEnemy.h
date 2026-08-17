// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VectorTestDummy.h"
#include "VectorEnemy.generated.h"

class AVectorOrganPickup;
class UDamageType;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVectorBreakableAnchorComponent;
class UVectorEnemyRangedAttackComponent;
enum class EVectorAnchorGroupSide : uint8;

/**
 * 敌人三型枚举（S04，灰盒期一个类三型配置，攻击行为分化后再拆子类）。
 */
UENUM(BlueprintType)
enum class EVectorEnemyArchetype : uint8
{
	/** 跳囊虫（轻）：成群、移动快、易失衡易击飞（质量 1.25）。 */
	LightHoppper,

	/** 甲壳犀（重）：移动慢、正面难推（质量 5.0），侧后方弱。 */
	HeavyRhinoBeetle,

	/** 角槌兽（冲）：追击中周期性冲锋（预警高亮 → 高速冲量），自身是"免费炮弹"。 */
	ChargerRammer,

	/** Mechanical shell that fires a slow weak-homing arc orb. */
	ArcShell,

	/** Light aerial unit alternating one and three corrosion-colored physical shots. */
	CorrosionDrone,
};

/**
 * 灰盒敌人基类：测试靶全部能力（受控冲量/稳定/碰撞连锁/质量档）+ AI 追击。
 *
 * AI 由 AVectorEnemyController（NavMesh 寻路）驱动；失衡/倒地或被冲量驱动时
 * 暂停 AI（物理状态优先，玩家推出去的"弹药"不被 AI 拉回）。
 * 灰盒表现复用质量档配色：轻=绿小 / 重=紫大 / 冲锋=橙三角（Charger 特殊）。
 */
UCLASS()
class VECTOR_API AVectorEnemy : public AVectorTestDummy
{
	GENERATED_BODY()

public:
	AVectorEnemy(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;

	/** PCG rooms use a local safety plane instead of waiting for the world's distant KillZ. */
	void ConfigureEncounterVoidRecovery(double FloorWorldZ, const FVector& DropRecoveryLocation);

	/**
	 * Arms a lethal hammer hit to launch this enemy as a short-lived projectile
	 * instead of destroying it immediately when health reaches zero.
	 */
	void PrepareForHammerLethalLaunch();
	void PrepareForVectorGunLethalLaunch();

	bool IsLethalLaunchCorpse() const { return bLethalLaunchDeathActive; }

	/** 敌人三型配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy")
	EVectorEnemyArchetype Archetype = EVectorEnemyArchetype::LightHoppper;

	/** 常态移动速度（WalkSpeed），cm/s（ApplyArchetypeConfiguration 时按三型设置基准值）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MoveSpeedCmPerSecond = 300.0;

	/**
	 * 每只实例的速度浮动比例（±，如 0.1 = ±10%）：同种怪追着追着因速度差异自然拉开队形，
	 * 避免挤成一团导致撞击效果不可见（对齐 PVZ 普通僵尸"速度不固定"）。
	 * 用比例而非绝对值，保证种间基准速度差距不被浮动吞掉（跳囊虫 420 始终快于角槌兽 320）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	double MoveSpeedVarianceRatio = 0.1;

	/** Maximum travel time for a hammer-killed enemy that has not hit anything. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Death", meta = (ClampMin = "0.1", Units = "s"))
	double LethalLaunchMaximumLifetimeSeconds = 1.75;

	/** Brief visual hold after the lethal projectile's first body/wall impact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Death", meta = (ClampMin = "0.0", Units = "s"))
	double LethalLaunchImpactDespawnDelaySeconds = 0.18;

	/** Greybox organ spawned on normal death or at a lethal projectile's final impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Enemy|Death")
	TSubclassOf<AVectorOrganPickup> OrganPickupClass;

	/** 失衡/倒地/冲量驱动期间是否应暂停 AI（控制器在 Tick 中查询）。 */
	virtual bool ShouldPauseAI() const;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	/** 近身攻击组件（P2：预警→扑击玩家；攻击中暂停追击）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy")
	TObjectPtr<class UVectorEnemyAttackComponent> AttackComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy")
	TObjectPtr<UVectorEnemyRangedAttackComponent> RangedAttackComponent;

	/** Heavy archetype's two discrete paired anchor groups. Disabled for other archetypes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Structure")
	TObjectPtr<UVectorBreakableAnchorComponent> BreakableAnchorComponent;

	/** Greybox left/right anchor silhouettes; imported creature meshes can replace only presentation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Structure|Presentation")
	TObjectPtr<UStaticMeshComponent> LeftAnchorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Structure|Presentation")
	TObjectPtr<UStaticMeshComponent> RightAnchorMesh;

	/** A collision-free silhouette cue that remains visible when imported meshes ignore tint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	TObjectPtr<UStaticMeshComponent> ArchetypeIndicatorMesh;

	/** Persistent low-intensity role color; attack warnings use the separate white pulse. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	TObjectPtr<UPointLightComponent> ArchetypeIndicatorLight;

	/** CC0 prototype silhouettes. Missing assets fall back to the established greybox cube. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	TSoftObjectPtr<UStaticMesh> LightPrototypeMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	TSoftObjectPtr<UStaticMesh> HeavyPrototypeMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	TSoftObjectPtr<UStaticMesh> ChargerPrototypeMesh;

	/** Bosses and future special enemies can replace the archetype silhouette without new subclasses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	TSoftObjectPtr<UStaticMesh> PrototypeMeshOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Enemy|Presentation")
	FVector PrototypeMeshScaleOverride = FVector::ZeroVector;

protected:
	virtual void BeginPlay() override;

	/** 按三型应用配置（质量/速度/呈现），BeginPlay 幂等。 */
	void ApplyArchetypeConfiguration();

	/** 生命归零回调（灰盒期销毁；正式期替换为倒地/掉落表现）。 */
	UFUNCTION()
	void HandleDeath();

private:
	virtual void Destroyed() override;

	void TriggerVoidRecovery(const TCHAR* Reason);
	void HandleLethalLaunchBodyImpact(AActor* OtherActor);
	void HandleLethalLaunchSurfaceImpact(double ImpactSpeedCmPerSecond);
	void ScheduleLethalLaunchDespawn(const TCHAR* Reason);
	void SpawnOrganDrop(const TCHAR* Reason);
	void HandleAnchorGroupBroken(EVectorAnchorGroupSide Side, int32 BrokenGroupCount);
	void UpdateAnchorPresentation();
	void ApplyPrototypeMeshPresentation();
	void UpdateArchetypeIndicatorPresentation();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ArchetypeIndicatorMaterial;

	bool bLethalLaunchArmed = false;
	bool bLethalLaunchDeathActive = false;
	bool bLethalLaunchImpactSeen = false;
	bool bOrganDropSpawned = false;
	bool bEncounterVoidRecoveryEnabled = false;
	bool bVoidRecoveryTriggered = false;
	double StructureBreakPulseSecondsRemaining = 0.0;
	double EncounterVoidRecoveryFloorZ = 0.0;
	FVector EncounterVoidDropLocation = FVector::ZeroVector;
};
