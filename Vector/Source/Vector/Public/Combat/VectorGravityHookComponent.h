// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/VectorActionTypes.h"
#include "VectorGravityHookComponent.generated.h"

class AActor;
class UVectorImpactCollisionComponent;

/** 双端绳线枪的世界状态；不是装备动作 Phase。 */
UENUM(BlueprintType)
enum class EVectorGravityHookMode : uint8
{
	None,
	AwaitingSecondEndpoint,
	PullingPlayerToAnchor,
	RetractingPair,
};

/**
 * 双端绳线枪（保留旧类名以避免角色组件资产迁移）：
 * - 第一发命中静态墙：玩家与墙面锚点连接，按住收绳、松开保留速度；
 * - 第一发命中怪物：记录端点 A；第二次按下手动选择怪物 B；
 * - A/B 配对后使用无弹性单向张力绳；侧向锤击触发长半径摆锤，之后卷扬缩短绳长；
 * - 绳线不写伤害；A/B 互撞或撞墙才断绳，擦碰第三者只短暂停绳并复用统一碰撞结算。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorGravityHookComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVectorGravityHookComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 绳线枪选中时左键按下：开始墙面牵引，或选择怪物第一/第二端点。 */
	void StartHook();

	/** 左键松开：仅墙面牵引会断开；怪物第一端点与活动配对不会被按键松开取消。 */
	void ReleaseHook();

	/** Switching equipment releases transient cable actions but preserves an active monster pair. */
	void HolsterHook();
	void CancelHook();

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	bool IsHookActive() const { return HookMode != EVectorGravityHookMode::None; }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	EVectorGravityHookMode GetHookMode() const { return HookMode; }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	AActor* GetHookedTarget() const { return FirstTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	AActor* GetSecondHookedTarget() const { return SecondTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	double GetCooldownSecondsRemaining() const { return CooldownSecondsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	bool IsOnCooldown() const { return CooldownSecondsRemaining > 0.0; }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	double GetPairSetupSecondsRemaining() const { return PairSetupSecondsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Vector|CableGun")
	double GetPairSwingSecondsRemaining() const { return PairSwingSecondsRemaining; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|CableGun")
	FVectorActionTimeline Timeline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	double HookRangeCm = 1800.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	double HookRadiusCm = 120.0;

	/** Small wall sweep radius: forgiving enough for mouse aim without selecting a different wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	double WallTargetingRadiusCm = 35.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Targeting", meta = (ClampMin = "0.1", Units = "s"))
	double SecondShotWindowSeconds = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	double MaximumPairDistanceCm = 1600.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Wall", meta = (ClampMin = "0.0", Units = "cm/s"))
	double PlayerReelSpeedCmPerSecond = 1500.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Wall", meta = (ClampMin = "0.0", Units = "cm"))
	double PlayerAnchorStopDistanceCm = 110.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Wall", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MaximumPlayerTangentialSpeedCmPerSecond = 1200.0;

	/** Low-braking coast after releasing/reaching a wall anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Wall", meta = (ClampMin = "0.0", Units = "s"))
	double WallReleaseMomentumCarrySeconds = 0.55;

	/** Winch speed after setup/swing. Slack rope does not pull until this length catches up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "cm/s"))
	double PairReelSpeedCmPerSecond = 260.0;

	/** Pair stays visible but does not retract, giving time for one hammer charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "s"))
	double PairSetupGraceSeconds = 1.5;

	/** Sideways hammer momentum pauses the winch so the long pair can sweep the crowd. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "s"))
	double HammerSwingDurationSeconds = 2.0;

	/** Small numerical allowance before the one-sided rope becomes taut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "cm"))
	double RopeConstraintToleranceCm = 3.0;

	/** Maximum non-elastic stretch correction; this is constraint stabilization, not spring force. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MaximumRopeCorrectionSpeedCmPerSecond = 600.0;

	/** 限制 h/r，防止绳长趋近零时切向速度数值爆炸。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "cm/s"))
	double MaximumRelativeTangentialSpeedCmPerSecond = 1800.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.1", Units = "s"))
	double MaximumPairLifetimeSeconds = 6.0;

	/** Briefly yield to a third-body rebound without breaking the A/B cable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0", Units = "s"))
	double IncidentalBodyImpactPauseSeconds = 0.12;

	/** 角动量的轻微空气/绳索损耗；0 表示理想守恒。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0"))
	double AngularMomentumDampingPerSecond = 0.08;

	/** 识别锤击/冲锋新注入的切向角动量，低于此差值视为数值噪声。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Pair", meta = (ClampMin = "0.0"))
	double AngularMomentumCaptureThreshold = 25000.0;

	/** Shared recovery after wall release, selection timeout, miss, or pair break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun", meta = (ClampMin = "0.0", Units = "s"))
	double CableCooldownSeconds = 0.60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Debug")
	bool bDrawHookDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Debug")
	bool bLogCablePhysics = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|CableGun|Debug", meta = (ClampMin = "0.05", Units = "s"))
	double DiagnosticLogIntervalSeconds = 0.25;

private:
	bool TryAcquireActionLock();
	void ReleaseActionLock();
	bool FindStaticAnchor(const FVector& AimDirection, FVector& OutAnchorPoint) const;
	void StartCooldown();
	void BeginFirstEndpoint(AActor* Target);
	bool TryBeginPair(AActor* Target);
	void BeginWallPull(const FVector& AnchorPoint);
	void UpdateFirstEndpoint(float DeltaTime);
	void UpdateWallPull();
	void BeginWallReleaseMomentumCarry();
	void UpdatePair(float DeltaTime);
	void FinishTransientAction(const TCHAR* Reason);
	void BreakPair(const TCHAR* Reason);
	void BindPairImpactDelegates();
	void ClearPairImpactDelegates();
	void HandleEndpointBodyImpact(AActor* OtherActor);
	void HandleEndpointSurfaceContact(double ImpactSpeedCmPerSecond);
	void DrawCableDebug() const;

	TWeakObjectPtr<AActor> FirstTarget;
	TWeakObjectPtr<AActor> SecondTarget;
	FVector WallAnchorPoint = FVector::ZeroVector;
	EVectorGravityHookMode HookMode = EVectorGravityHookMode::None;
	double SecondShotSecondsRemaining = 0.0;
	double PairSecondsRemaining = 0.0;
	double PairSetupSecondsRemaining = 0.0;
	double PairSwingSecondsRemaining = 0.0;
	double PairReelPauseSecondsRemaining = 0.0;
	double PairCableLengthCm = 0.0;
	double CooldownSecondsRemaining = 0.0;
	double PairSpecificAngularMomentum = 0.0;
	double DiagnosticLogSecondsRemaining = 0.0;
	bool bPairImpactSeen = false;
	bool bOwnsActionLock = false;
	FDelegateHandle FirstImpactDelegateHandle;
	FDelegateHandle SecondImpactDelegateHandle;
	FDelegateHandle FirstSurfaceContactDelegateHandle;
	FDelegateHandle SecondSurfaceContactDelegateHandle;
};
