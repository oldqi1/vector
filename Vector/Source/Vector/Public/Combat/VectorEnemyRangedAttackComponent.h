// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorEnemyRangedAttackComponent.generated.h"

class APawn;
class AVectorKineticOrb;

UENUM(BlueprintType)
enum class EVectorEnemyRangedPattern : uint8
{
	None,
	ArcWeakHoming,
	CorrosionVolley,
};

/** Pure volley grammar shared by pre-fire telegraphs, release, and Automation. */
struct VECTOR_API FVectorEnemyRangedPatternMath
{
	static int32 GetProjectileCount(
		EVectorEnemyRangedPattern Pattern,
		int32 SequenceIndex);

	static double GetSpreadAngleDegrees(
		int32 ProjectileIndex,
		int32 ProjectileCount,
		double MaximumSpreadDegrees);
};

/**
 * Readable ranged enemy grammar built from normal physical targets.
 * Projectiles can be redirected by the Vector Gun and keep using the shared
 * collision/mass model; this component does not apply hidden hitscan damage.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorEnemyRangedAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorEnemyRangedAttackComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void ConfigurePattern(EVectorEnemyRangedPattern NewPattern);
	bool IsCommittingAttack() const;
	EVectorEnemyRangedPattern GetPattern() const { return Pattern; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|EnemyRanged")
	TSubclassOf<AVectorKineticOrb> ProjectileClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|EnemyRanged")
	EVectorEnemyRangedPattern Pattern = EVectorEnemyRangedPattern::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|EnemyRanged", meta = (Units = "cm"))
	double AttackRangeCm = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|EnemyRanged", meta = (Units = "s"))
	double WarmupSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|EnemyRanged", meta = (Units = "s"))
	double CooldownSeconds = 0.0;

	/** A readable interruption earns safety, but cannot be followed by an instant retry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|EnemyRanged", meta = (Units = "s", ClampMin = "0.0"))
	double InterruptedRetrySeconds = 0.75;

private:
	enum class EPhase : uint8
	{
		Idle,
		Warmup,
		Cooldown,
	};

	APawn* FindPlayerPawn() const;
	void BeginWarmup(APawn* PlayerPawn);
	void ReleaseProjectiles(APawn* PlayerPawn);
	void CancelWarmup(const TCHAR* Reason);
	void DrawTelegraph(const APawn* PlayerPawn) const;
	bool SpawnProjectile(
		APawn* PlayerPawn,
		const FVector& Direction,
		bool bWeakHoming);

	EPhase Phase = EPhase::Idle;
	double PhaseSecondsRemaining = 0.0;
	double ProjectileSpeedCmPerSecond = 0.0;
	double ProjectileLifetimeSeconds = 0.0;
	double GuidanceSeconds = 0.0;
	double GuidanceTurnRateDegreesPerSecond = 0.0;
	double VolleySpreadDegrees = 0.0;
	int32 AttackSequenceIndex = 0;
};
