// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VectorCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class UAnimSequence;
class UVectorImpulseHammerComponent;
class UVectorGravityHookComponent;
class UVectorActionLockComponent;
class UVectorHealthComponent;
class UVectorPhysicsModifierComponent;
class UVectorModifierApplicatorComponent;
class UVectorLiftForkComponent;

/** EnhancedInput 值类型；注意是 struct（USTRUCT），须按引用前向声明。 */
struct FInputActionValue;

/** Crashlands-style active equipment bar: number keys select, LMB uses. */
UENUM(BlueprintType)
enum class EVectorEquipmentSlot : uint8
{
	Hammer,
	CableGun,
	Lubricant,
	BuoyantSpore,
	LiftFork,
};

/**
 * 冲量荒原默认平面俯视角 Pawn。
 *
 * 玩家角色：双腿机器人 SK_Robot（SkeletalMesh）+ 固定 55° 俯角相机 + 有限水平旋转
 * （右键拖动/右摇杆 Yaw，角色朝向跟随移动方向）+ 镜头相对 WASD 移动 + 运行时构造 EnhancedInput
 * + 数字键 1-5 选择装备、左键统一使用，全部共享动作锁。
 * 动画：Idle / Run（按水平速度切换），Falling 时切 Jump。
 */
UCLASS()
class VECTOR_API AVectorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVectorCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Vector|Equipment")
	EVectorEquipmentSlot GetSelectedEquipmentSlot() const { return SelectedEquipmentSlot; }

	UFUNCTION(BlueprintPure, Category = "Vector|Equipment")
	FString GetSelectedEquipmentLabel() const;

protected:
	/** 相机固定俯角，单位 deg；原型期不响应鼠标 Pitch。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Camera", meta = (ClampMin = "10.0", ClampMax = "80.0", Units = "deg"))
	float CameraDownwardPitchDegrees = 55.0f;

	/** 右键/中键拖动水平旋转灵敏度，单位 deg / raw Mouse2D input unit。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Camera", meta = (ClampMin = "0.0", Units = "deg"))
	float MouseLookYawDegreesPerUnit = 0.20f;

	/** 相机臂长度，单位 cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Camera", meta = (ClampMin = "100.0", Units = "cm"))
	float CameraArmLengthCm = 1400.0f;

	/** 滚轮缩放下限，cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Camera", meta = (ClampMin = "100.0", Units = "cm"))
	double CameraZoomMinCm = 600.0;

	/** 滚轮缩放上限，cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Camera", meta = (ClampMin = "100.0", Units = "cm"))
	double CameraZoomMaxCm = 2400.0;

	/** 滚轮每格缩放的相机臂变化量，cm。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Camera", meta = (ClampMin = "10.0", Units = "cm"))
	double CameraZoomStepCm = 150.0;

	/** 跳跃初速度 Z 分量，cm/s（默认 600 → 跳高约 1.8m）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float JumpZVelocityCmPerSecond = 600.0f;

	/** 水平速度超过该值切换到 Run 动画，单位 cm/s。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector|Presentation", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RunAnimationSpeedThresholdCmPerSecond = 20.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 双腿机器人 Idle 循环。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Presentation")
	TObjectPtr<UAnimSequence> RobotIdleAnimation;

	/** 双腿机器人 Run 循环。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Presentation")
	TObjectPtr<UAnimSequence> RobotRunAnimation;

	/** 双腿机器人 Jump（Falling 期间循环；落地自动切回）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vector|Presentation")
	TObjectPtr<UAnimSequence> RobotJumpAnimation;

	/** 冲量锤：左键蓄力→水平冲量（推目标/扣稳定度）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Combat")
	TObjectPtr<UVectorImpulseHammerComponent> ImpulseHammer;

	/** 双端绳线枪：按住墙面牵引玩家；对怪物手动双发后收绳对撞。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Combat")
	TObjectPtr<UVectorGravityHookComponent> GravityHook;

	/** 玩家级装备动作互斥锁。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Combat")
	TObjectPtr<UVectorActionLockComponent> ActionLock;

	/** 装备栏 3/4：对准目标施加润滑剂/浮空孢子。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Combat")
	TObjectPtr<UVectorModifierApplicatorComponent> ModifierApplicator;

	/** 装备栏 5：对准近处目标施加垂直冲量。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Combat")
	TObjectPtr<UVectorLiftForkComponent> LiftFork;

	/** 玩家核心生命（100 血；被敌人撞击/扑击扣血，归零死亡重生）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Combat")
	TObjectPtr<UVectorHealthComponent> HealthComponent;

	/** 玩家也遵守统一环境/调质物理属性契约（低摩擦区会读取）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vector|Physics")
	TObjectPtr<UVectorPhysicsModifierComponent> PhysicsModifierComponent;

private:
	/** 必要时构造运行时输入上下文并绑定。 */
	void EnsureRuntimeInput();

	/** WASD / 左摇杆 → 镜头相对移动。 */
	void HandleMoveInput(const FInputActionValue& Value);

	/** 按住右键（或中键）拖动 / 右摇杆 X → 相机 Yaw；普通鼠标移动只负责光标瞄准。 */
	void HandleLookInput(const FInputActionValue& Value);
	void HandleCameraRotatePressed();
	void HandleCameraRotateReleased();

	/** 左键按下/松开：使用当前装备。 */
	void HandleAttackPressed();
	void HandleAttackReleased();

	/** 数字键 1-5：选择装备。 */
	void HandleSelectHammerPressed();
	void HandleHookPressed();
	void HandleLubricantPressed();
	void HandleBuoyantSporePressed();
	void HandleLiftForkPressed();
	void SelectEquipment(EVectorEquipmentSlot NewEquipmentSlot);

	/** 空格按下：起跳。 */
	void HandleJumpPressed();

	/** 空格松开：停止起跳。 */
	void HandleJumpReleased();

	/** 滚轮：镜头缩放（饥荒式俯视角远近）。 */
	void HandleZoomInput(const FInputActionValue& Value);

	/** 按水平速度 / Falling 切换 Idle / Run / Jump；只在目标动画变化时触发 PlayAnimation。 */
	void UpdateRobotAnimation();

	/** 玩家生命归零：灰盒重生（回原点 + 满血）。 */
	UFUNCTION()
	void HandlePlayerDeath();

	/** 当前正在播放的动画；用于去重避免每帧重复触发。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentRobotAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CameraRotateInputAction;

	/** 左键攻击动作（冲量锤蓄力/释放）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AttackInputAction;

	/** 数字键 1：冲量锤。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectHammerInputAction;

	/** 数字键 2：双端绳线枪。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HookInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LubricantInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BuoyantSporeInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LiftForkInputAction;

	/** 跳跃动作（空格）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpInputAction;

	/** 缩放动作（滚轮）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ZoomInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MoveInputMappingContext;

	bool bCameraRotateHeld = false;
	int32 SavedCursorX = 0;
	int32 SavedCursorY = 0;

	UPROPERTY(VisibleAnywhere, Category = "Vector|Equipment")
	EVectorEquipmentSlot SelectedEquipmentSlot = EVectorEquipmentSlot::Hammer;

	/** 灰盒重生点：BeginPlay 时实际出生 Transform（通常来自 PlayerStart）。 */
	FTransform RespawnTransform = FTransform::Identity;
};
