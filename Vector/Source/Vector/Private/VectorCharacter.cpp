// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Combat/VectorActionLockComponent.h"
#include "Combat/VectorGravityHookComponent.h"
#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpulseHammerComponent.h"
#include "Combat/VectorLiftForkComponent.h"
#include "Combat/VectorModifierApplicatorComponent.h"
#include "Combat/VectorGunComponent.h"
#include "Combat/VectorTrajectoryPreviewComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Physics/VectorPhysicsModifierComponent.h"
#include "Progression/VectorRunProgressionComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorPlayer, Log, All);

AVectorCharacter::AVectorCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVectorCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// 俯视角移动游戏：角色朝向跟随移动方向；鼠标瞄准，中键拖动才控制镜头 Yaw。
	// 不绑相机 Yaw，否则 WASD 会变成"面朝前的平移"（倒走/侧走），Run 动画与运动方向不一致。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 540.0, 0.0);
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocityCmPerSecond;

	// ---- 双腿机器人外观（移植自 MorphorbitGravityCharacter 构造 L92-142，直接可见） ----
	USkeletalMeshComponent* RobotMesh = GetMesh();
	RobotMesh->SetupAttachment(GetCapsuleComponent());
	RobotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RobotMesh->SetGenerateOverlapEvents(false);
	// 平面原型沿用 AlwaysTickPoseAndRefreshBones，防止远相机下动画不更新。
	RobotMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	// Robot 源高度约 450 cm；0.4 倍后与既有 192 cm 高 Capsule 接近，脚底仍落在胶囊底部。
	RobotMesh->SetRelativeScale3D(FVector(0.4f));
	RobotMesh->SetRelativeLocation(
		FVector(0.0, 0.0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	// 源 FBX 的可见前方是 +Y，而 CharacterMovement 以 Actor +X 为前方；只补偿网格轴，不旋转规则 Actor。
	RobotMesh->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	RobotMesh->SetVisibility(true);
	RobotMesh->SetHiddenInGame(false);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotMeshAsset(
		TEXT("/Game/Prototype/Character/Robot/SK_Robot.SK_Robot"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RobotIdleAsset(
		TEXT("/Game/Prototype/Character/Robot/A_Robot_Idle.A_Robot_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RobotRunAsset(
		TEXT("/Game/Prototype/Character/Robot/A_Robot_Run.A_Robot_Run"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RobotJumpAsset(
		TEXT("/Game/Prototype/Character/Robot/A_Robot_Jump.A_Robot_Jump"));

	if (RobotMeshAsset.Succeeded())
	{
		RobotMesh->SetSkeletalMeshAsset(RobotMeshAsset.Object);
	}
	RobotIdleAnimation = RobotIdleAsset.Object;
	RobotRunAnimation = RobotRunAsset.Object;
	RobotJumpAnimation = RobotJumpAsset.Object;
	if (RobotIdleAnimation)
	{
		// 构造期必须写入可序列化 AnimationData；PlayAnimation 的状态只适合 BeginPlay 之后。
		RobotMesh->OverrideAnimationData(RobotIdleAnimation, true, true, 0.0f, 1.0f);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = CameraArmLengthCm;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 玩家级动作锁先于装备创建；左右键装备共享它，拒绝同帧速度竞争。
	ActionLock = CreateDefaultSubobject<UVectorActionLockComponent>(TEXT("ActionLock"));

	// 冲量锤装备（S02）：左键按住蓄力、松开释放水平冲量。
	ImpulseHammer = CreateDefaultSubobject<UVectorImpulseHammerComponent>(TEXT("ImpulseHammer"));
	VectorGun = CreateDefaultSubobject<UVectorGunComponent>(TEXT("VectorGun"));
	TrajectoryPreview = CreateDefaultSubobject<UVectorTrajectoryPreviewComponent>(TEXT("TrajectoryPreview"));
	GravityHook = CreateDefaultSubobject<UVectorGravityHookComponent>(TEXT("GravityHook"));
	ModifierApplicator = CreateDefaultSubobject<UVectorModifierApplicatorComponent>(TEXT("ModifierApplicator"));
	LiftFork = CreateDefaultSubobject<UVectorLiftForkComponent>(TEXT("LiftFork"));
	RunProgression = CreateDefaultSubobject<UVectorRunProgressionComponent>(TEXT("RunProgression"));

	// 玩家核心生命（P2 补全）：被敌人撞击/扑击扣血，归零重生。
	HealthComponent = CreateDefaultSubobject<UVectorHealthComponent>(TEXT("Health"));
	PhysicsModifierComponent = CreateDefaultSubobject<UVectorPhysicsModifierComponent>(TEXT("PhysicsModifier"));
}

void AVectorCharacter::BeginPlay()
{
	Super::BeginPlay();
	RespawnTransform = GetActorTransform();
	RefreshVoidRecoveryFloor();

	// 固定倾角：只保留 Yaw 作为玩家控制的水平旋转自由度。
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator InitialRotation = PC->GetControlRotation();
		InitialRotation.Pitch = -CameraDownwardPitchDegrees;
		InitialRotation.Roll = 0.0;
		PC->SetControlRotation(InitialRotation);
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			EnsureRuntimeInput();
			Subsystem->AddMappingContext(MoveInputMappingContext, 0);
		}
	}

	// 玩家生命归零 → 灰盒期重生（复位位置 + 回满血）。
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AVectorCharacter::HandlePlayerDeath);
	}
}

void AVectorCharacter::HandlePlayerDeath()
{
	// 灰盒重生：回到实际 PlayerStart + 回满血 + 清冲量（正式期：死亡表现/掉局内资源）。
	StopJumping();
	if (HealthComponent)
	{
		HealthComponent->ResetHealth();
	}
	if (UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		// Clear first so the impulse-preserving StopMovement override takes the
		// normal hard-stop path. Otherwise the pawn can retain its current launch
		// velocity and keep sliding/flying after TeleportTo.
		Movement->ClearQueuedWorldVelocityChanges();
		Movement->StopMovementImmediately();
	}
	if (ImpulseHammer)
	{
		ImpulseHammer->CancelAction();
	}
	if (VectorGun)
	{
		VectorGun->CancelAction();
	}
	if (GravityHook)
	{
		GravityHook->CancelHook();
	}
	if (ModifierApplicator)
	{
		ModifierApplicator->CancelAction();
	}
	if (LiftFork)
	{
		LiftFork->CancelAction();
	}
	if (ActionLock)
	{
		ActionLock->ForceRelease();
	}
	const bool bTeleported = TeleportTo(
		RespawnTransform.GetLocation(), RespawnTransform.Rotator());
	if (UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	bVoidRecoveryInProgress = false;
	UE_LOG(LogTemp, Log, TEXT("Player died -> respawn at %s teleported=%s velocity=%s mode=%s"),
		*RespawnTransform.GetLocation().ToCompactString(),
		bTeleported ? TEXT("YES") : TEXT("FAILED"),
		*GetVelocity().ToCompactString(),
		GetCharacterMovement() ? *GetCharacterMovement()->GetMovementName() : TEXT("(none)"));
}

bool AVectorCharacter::SetRespawnCheckpoint(const FTransform& NewRespawnTransform)
{
	if (NewRespawnTransform.ContainsNaN())
	{
		return false;
	}
	RespawnTransform = NewRespawnTransform;
	RespawnTransform.SetScale3D(FVector::OneVector);
	RefreshVoidRecoveryFloor();
	UE_LOG(LogTemp, Log, TEXT("Player checkpoint updated: location=%s rotation=%s"),
		*RespawnTransform.GetLocation().ToCompactString(),
		*RespawnTransform.Rotator().ToCompactString());
	return true;
}

void AVectorCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bVoidRecoveryInProgress
		&& HealthComponent
		&& !HealthComponent->IsDead()
		&& GetActorLocation().Z < ActiveVoidRecoveryFloorWorldZ)
	{
		TriggerVoidRecovery(TEXT("checkpoint_floor"));
	}
	UpdateRobotAnimation();
}

void AVectorCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!bVoidRecoveryInProgress)
	{
		TriggerVoidRecovery(TEXT("world_kill_z"));
		return;
	}

	Super::FellOutOfWorld(DamageType);
}

void AVectorCharacter::TriggerVoidRecovery(const TCHAR* TriggerReason)
{
	if (bVoidRecoveryInProgress)
	{
		return;
	}

	bVoidRecoveryInProgress = true;
	UE_LOG(LogVectorPlayer, Warning,
		TEXT("Player void recovery: reason=%s location=%s floorZ=%.0f checkpoint=%s encounterLedger=UNCHANGED"),
		TriggerReason,
		*GetActorLocation().ToCompactString(),
		ActiveVoidRecoveryFloorWorldZ,
		*RespawnTransform.GetLocation().ToCompactString());

	// Reuse the combat-death cleanup path so health, queued velocity, active
	// equipment and the action lock cannot survive the checkpoint teleport.
	// Player death is intentionally unrelated to the enemy encounter ledger.
	if (HealthComponent && !HealthComponent->IsDead())
	{
		HealthComponent->ApplyDamage(FMath::Max(1.0, HealthComponent->GetHealth()));
	}
	else
	{
		HandlePlayerDeath();
	}
}

void AVectorCharacter::RefreshVoidRecoveryFloor()
{
	ActiveVoidRecoveryFloorWorldZ = RespawnTransform.GetLocation().Z
		- FMath::Max(100.0, VoidRecoveryDropBelowCheckpointCm);
	UE_LOG(LogVectorPlayer, Verbose,
		TEXT("Player void floor updated: checkpointZ=%.0f drop=%.0f floorZ=%.0f"),
		RespawnTransform.GetLocation().Z,
		VoidRecoveryDropBelowCheckpointCm,
		ActiveVoidRecoveryFloorWorldZ);
}

void AVectorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureRuntimeInput();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AVectorCharacter::HandleMoveInput);
		EnhancedInput->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AVectorCharacter::HandleLookInput);
		EnhancedInput->BindAction(CameraRotateInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleCameraRotatePressed);
		EnhancedInput->BindAction(CameraRotateInputAction, ETriggerEvent::Completed, this, &AVectorCharacter::HandleCameraRotateReleased);
		EnhancedInput->BindAction(AttackInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleAttackPressed);
		EnhancedInput->BindAction(AttackInputAction, ETriggerEvent::Completed, this, &AVectorCharacter::HandleAttackReleased);
		EnhancedInput->BindAction(SelectHammerInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleSelectHammerPressed);
		EnhancedInput->BindAction(HookInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleHookPressed);
		EnhancedInput->BindAction(LubricantInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleLubricantPressed);
		EnhancedInput->BindAction(BuoyantSporeInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleBuoyantSporePressed);
		EnhancedInput->BindAction(LiftForkInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleLiftForkPressed);
		EnhancedInput->BindAction(CalibrationChoiceOneInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleCalibrationChoiceOnePressed);
		EnhancedInput->BindAction(CalibrationChoiceTwoInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleCalibrationChoiceTwoPressed);
		EnhancedInput->BindAction(CalibrationChoiceThreeInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleCalibrationChoiceThreePressed);
		EnhancedInput->BindAction(PauseInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandlePausePressed);
		EnhancedInput->BindAction(QuitInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleQuitPressed);
		EnhancedInput->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleJumpPressed);
		EnhancedInput->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AVectorCharacter::HandleJumpReleased);
		EnhancedInput->BindAction(ZoomInputAction, ETriggerEvent::Triggered, this, &AVectorCharacter::HandleZoomInput);
	}
}

void AVectorCharacter::EnsureRuntimeInput()
{
	if (MoveInputAction && LookInputAction && CameraRotateInputAction
		&& AttackInputAction && SelectHammerInputAction && HookInputAction
		&& LubricantInputAction && BuoyantSporeInputAction && LiftForkInputAction
		&& CalibrationChoiceOneInputAction && CalibrationChoiceTwoInputAction
		&& CalibrationChoiceThreeInputAction && PauseInputAction && QuitInputAction
		&& JumpInputAction && ZoomInputAction && MoveInputMappingContext)
	{
		return;
	}

	MoveInputAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveInputAction->ValueType = EInputActionValueType::Axis2D;

	LookInputAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookInputAction->ValueType = EInputActionValueType::Axis2D;

	CameraRotateInputAction = NewObject<UInputAction>(this, TEXT("IA_CameraRotate"));
	CameraRotateInputAction->ValueType = EInputActionValueType::Boolean;

	AttackInputAction = NewObject<UInputAction>(this, TEXT("IA_Attack"));
	AttackInputAction->ValueType = EInputActionValueType::Boolean;

	SelectHammerInputAction = NewObject<UInputAction>(this, TEXT("IA_SelectHammer"));
	SelectHammerInputAction->ValueType = EInputActionValueType::Boolean;

	HookInputAction = NewObject<UInputAction>(this, TEXT("IA_SelectCable"));
	HookInputAction->ValueType = EInputActionValueType::Boolean;

	LubricantInputAction = NewObject<UInputAction>(this, TEXT("IA_Lubricant"));
	LubricantInputAction->ValueType = EInputActionValueType::Boolean;

	BuoyantSporeInputAction = NewObject<UInputAction>(this, TEXT("IA_BuoyantSpore"));
	BuoyantSporeInputAction->ValueType = EInputActionValueType::Boolean;

	LiftForkInputAction = NewObject<UInputAction>(this, TEXT("IA_LiftFork"));
	LiftForkInputAction->ValueType = EInputActionValueType::Boolean;

	CalibrationChoiceOneInputAction = NewObject<UInputAction>(this, TEXT("IA_CalibrationChoiceOne"));
	CalibrationChoiceOneInputAction->ValueType = EInputActionValueType::Boolean;
	CalibrationChoiceTwoInputAction = NewObject<UInputAction>(this, TEXT("IA_CalibrationChoiceTwo"));
	CalibrationChoiceTwoInputAction->ValueType = EInputActionValueType::Boolean;
	CalibrationChoiceThreeInputAction = NewObject<UInputAction>(this, TEXT("IA_CalibrationChoiceThree"));
	CalibrationChoiceThreeInputAction->ValueType = EInputActionValueType::Boolean;
	PauseInputAction = NewObject<UInputAction>(this, TEXT("IA_Pause"));
	PauseInputAction->ValueType = EInputActionValueType::Boolean;
	PauseInputAction->bTriggerWhenPaused = true;
	QuitInputAction = NewObject<UInputAction>(this, TEXT("IA_QuitWhilePaused"));
	QuitInputAction->ValueType = EInputActionValueType::Boolean;
	QuitInputAction->bTriggerWhenPaused = true;

	JumpInputAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	JumpInputAction->ValueType = EInputActionValueType::Boolean;

	ZoomInputAction = NewObject<UInputAction>(this, TEXT("IA_Zoom"));
	ZoomInputAction->ValueType = EInputActionValueType::Axis1D;

	MoveInputMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MoveInputMappingContext->MapKey(MoveInputAction, EKeys::D);

	FEnhancedActionKeyMapping& MoveLeft = MoveInputMappingContext->MapKey(MoveInputAction, EKeys::A);
	MoveLeft.Modifiers.Add(NewObject<UInputModifierNegate>(MoveInputMappingContext));

	FEnhancedActionKeyMapping& MoveForward = MoveInputMappingContext->MapKey(MoveInputAction, EKeys::W);
	MoveForward.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MoveInputMappingContext));

	FEnhancedActionKeyMapping& MoveBackward = MoveInputMappingContext->MapKey(MoveInputAction, EKeys::S);
	MoveBackward.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MoveInputMappingContext));
	MoveBackward.Modifiers.Add(NewObject<UInputModifierNegate>(MoveInputMappingContext));

	MoveInputMappingContext->MapKey(MoveInputAction, EKeys::Gamepad_Left2D);

	FEnhancedActionKeyMapping& MouseLook = MoveInputMappingContext->MapKey(LookInputAction, EKeys::Mouse2D);
	UInputModifierScalar* MouseScale = NewObject<UInputModifierScalar>(MoveInputMappingContext);
	MouseScale->Scalar = FVector(MouseLookYawDegreesPerUnit, 0.0f, 1.0f);
	MouseLook.Modifiers.Add(MouseScale);

	FEnhancedActionKeyMapping& GamepadLook = MoveInputMappingContext->MapKey(LookInputAction, EKeys::Gamepad_Right2D);
	UInputModifierScalar* GamepadScale = NewObject<UInputModifierScalar>(MoveInputMappingContext);
	GamepadScale->Scalar = FVector(90.0f, 0.0f, 1.0f);
	GamepadLook.Modifiers.Add(GamepadScale);
	MoveInputMappingContext->MapKey(CameraRotateInputAction, EKeys::RightMouseButton);
	MoveInputMappingContext->MapKey(CameraRotateInputAction, EKeys::MiddleMouseButton);

	// Crashlands-style equipment bar: numbers select, LMB uses the selected tool.
	MoveInputMappingContext->MapKey(AttackInputAction, EKeys::LeftMouseButton);
	MoveInputMappingContext->MapKey(SelectHammerInputAction, EKeys::One);
	MoveInputMappingContext->MapKey(HookInputAction, EKeys::Two);
	MoveInputMappingContext->MapKey(LubricantInputAction, EKeys::Three);
	MoveInputMappingContext->MapKey(BuoyantSporeInputAction, EKeys::Four);
	MoveInputMappingContext->MapKey(LiftForkInputAction, EKeys::Five);
	MoveInputMappingContext->MapKey(CalibrationChoiceOneInputAction, EKeys::Z);
	MoveInputMappingContext->MapKey(CalibrationChoiceTwoInputAction, EKeys::X);
	MoveInputMappingContext->MapKey(CalibrationChoiceThreeInputAction, EKeys::C);
	MoveInputMappingContext->MapKey(PauseInputAction, EKeys::Escape);
	MoveInputMappingContext->MapKey(QuitInputAction, EKeys::Q);

	// 空格：跳跃（躲避冲锋/跨障）。
	MoveInputMappingContext->MapKey(JumpInputAction, EKeys::SpaceBar);
	MoveInputMappingContext->MapKey(JumpInputAction, EKeys::Gamepad_FaceButton_Bottom);

	// 滚轮：镜头缩放（饥荒式俯视角远近）。
	MoveInputMappingContext->MapKey(ZoomInputAction, EKeys::MouseWheelAxis);
}

void AVectorCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (MoveInput.IsNearlyZero() || !GetController())
	{
		return;
	}

	// 镜头相对移动：以相机 Yaw（水平旋转）为基准，把 WASD 投影到世界水平面。
	const FRotator CameraYaw(0.0, GetControlRotation().Yaw, 0.0);
	const FVector Forward = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MoveInput.Y);
	AddMovementInput(Right, MoveInput.X);
}

void AVectorCharacter::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const bool bGamepadRotation = PlayerController
		&& FMath::Abs(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightX)) > 0.01f;
	if (LookInput.IsNearlyZero() || !GetController()
		|| (!bCameraRotateHeld && !bGamepadRotation))
	{
		return;
	}

	// 有限水平旋转：只消费 X（Yaw）；Pitch 恒为固定俯角，不响应鼠标 Y。
	AddControllerYawInput(LookInput.X);
}

void AVectorCharacter::HandleCameraRotatePressed()
{
	bCameraRotateHeld = true;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		float CursorX = 0.0f;
		float CursorY = 0.0f;
		if (PlayerController->GetMousePosition(CursorX, CursorY))
		{
			SavedCursorX = FMath::RoundToInt(CursorX);
			SavedCursorY = FMath::RoundToInt(CursorY);
		}
		PlayerController->bShowMouseCursor = false;
	}
}

void AVectorCharacter::HandleCameraRotateReleased()
{
	bCameraRotateHeld = false;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->bShowMouseCursor = true;
		PlayerController->SetMouseLocation(SavedCursorX, SavedCursorY);
	}
}

void AVectorCharacter::HandleAttackPressed()
{
	switch (SelectedEquipmentSlot)
	{
	case EVectorEquipmentSlot::Hammer:
		if (VectorGun)
		{
			VectorGun->Fire();
		}
		break;
	case EVectorEquipmentSlot::CableGun:
		if (GravityHook)
		{
			GravityHook->StartHook();
		}
		break;
	case EVectorEquipmentSlot::Lubricant:
		if (ModifierApplicator)
		{
			ModifierApplicator->ApplyLubricant();
		}
		break;
	case EVectorEquipmentSlot::BuoyantSpore:
		if (ModifierApplicator)
		{
			ModifierApplicator->ApplyBuoyantSpore();
		}
		break;
	case EVectorEquipmentSlot::LiftFork:
		if (LiftFork)
		{
			LiftFork->BeginForkGesture();
		}
		break;
	default:
		break;
	}
}

void AVectorCharacter::HandleAttackReleased()
{
	if (SelectedEquipmentSlot == EVectorEquipmentSlot::CableGun && GravityHook)
	{
		GravityHook->ReleaseHook();
	}
	else if (SelectedEquipmentSlot == EVectorEquipmentSlot::LiftFork && LiftFork)
	{
		LiftFork->ReleaseForkGesture();
	}
}

void AVectorCharacter::HandleSelectHammerPressed()
{
	SelectEquipment(EVectorEquipmentSlot::Hammer);
}

void AVectorCharacter::HandleHookPressed()
{
	SelectEquipment(EVectorEquipmentSlot::CableGun);
}

void AVectorCharacter::HandleLubricantPressed()
{
	SelectEquipment(EVectorEquipmentSlot::Lubricant);
}

void AVectorCharacter::HandleBuoyantSporePressed()
{
	SelectEquipment(EVectorEquipmentSlot::BuoyantSpore);
}

void AVectorCharacter::HandleLiftForkPressed()
{
	SelectEquipment(EVectorEquipmentSlot::LiftFork);
}

void AVectorCharacter::HandleCalibrationChoiceOnePressed()
{
	if (RunProgression)
	{
		RunProgression->SelectPendingChoice(0);
	}
}

void AVectorCharacter::HandleCalibrationChoiceTwoPressed()
{
	if (RunProgression)
	{
		RunProgression->SelectPendingChoice(1);
	}
}

void AVectorCharacter::HandleCalibrationChoiceThreePressed()
{
	if (RunProgression)
	{
		RunProgression->SelectPendingChoice(2);
	}
}

void AVectorCharacter::HandlePausePressed()
{
	const bool bWasPaused = UGameplayStatics::IsGamePaused(this);
	const bool bChanged = UGameplayStatics::SetGamePaused(this, !bWasPaused);
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->bShowMouseCursor = true;
	}
	UE_LOG(LogVectorPlayer, Log,
		TEXT("Pause toggled: requested=%s changed=%s"),
		bWasPaused ? TEXT("RESUME") : TEXT("PAUSE"),
		bChanged ? TEXT("YES") : TEXT("no"));
}

void AVectorCharacter::HandleQuitPressed()
{
	if (!UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UE_LOG(LogVectorPlayer, Log, TEXT("Quit requested from pause overlay"));
	UKismetSystemLibrary::QuitGame(
		this, PlayerController, EQuitPreference::Quit, false);
}

void AVectorCharacter::SelectEquipment(const EVectorEquipmentSlot NewEquipmentSlot)
{
	if (SelectedEquipmentSlot == NewEquipmentSlot)
	{
		return;
	}

	if (SelectedEquipmentSlot == EVectorEquipmentSlot::CableGun && GravityHook)
	{
		GravityHook->HolsterHook();
	}
	else if (SelectedEquipmentSlot == EVectorEquipmentSlot::LiftFork && LiftFork)
	{
		LiftFork->CancelAction();
	}

	SelectedEquipmentSlot = NewEquipmentSlot;
	UE_LOG(LogTemp, Log, TEXT("Equipment selected: %s"),
		*GetSelectedEquipmentLabel());
}

FString AVectorCharacter::GetSelectedEquipmentLabel() const
{
	switch (SelectedEquipmentSlot)
	{
	case EVectorEquipmentSlot::Hammer:
		return TEXT("1 VECTOR GUN");
	case EVectorEquipmentSlot::CableGun:
		return TEXT("2 CABLE");
	case EVectorEquipmentSlot::Lubricant:
		return TEXT("3 LUBE");
	case EVectorEquipmentSlot::BuoyantSpore:
		return TEXT("4 FLOAT");
	case EVectorEquipmentSlot::LiftFork:
		return TEXT("5 LIFT");
	default:
		return TEXT("UNKNOWN");
	}
}

void AVectorCharacter::HandleJumpPressed()
{
	Jump();
}

void AVectorCharacter::HandleJumpReleased()
{
	StopJumping();
}

void AVectorCharacter::HandleZoomInput(const FInputActionValue& Value)
{
	if (!CameraBoom)
	{
		return;
	}

	// 滚轮上（正值）拉近，滚轮下（负值）拉远；钳制在 [Min, Max]。
	const float WheelValue = Value.Get<float>();
	const double TargetLength = FMath::Clamp(
		static_cast<double>(CameraBoom->TargetArmLength) - static_cast<double>(WheelValue) * CameraZoomStepCm,
		CameraZoomMinCm,
		CameraZoomMaxCm);
	CameraBoom->TargetArmLength = TargetLength;
}

void AVectorCharacter::UpdateRobotAnimation()
{
	USkeletalMeshComponent* RobotMesh = GetMesh();
	if (!RobotMesh)
	{
		return;
	}

	UAnimSequence* DesiredAnimation = RobotIdleAnimation;
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling() && RobotJumpAnimation)
	{
		DesiredAnimation = RobotJumpAnimation;
	}
	else if (GetVelocity().Size2D() > RunAnimationSpeedThresholdCmPerSecond && RobotRunAnimation)
	{
		DesiredAnimation = RobotRunAnimation;
	}

	// 只在目标动画变化时触发，避免每帧重复 PlayAnimation 重置播放位置。
	if (DesiredAnimation != CurrentRobotAnimation)
	{
		CurrentRobotAnimation = DesiredAnimation;
		if (DesiredAnimation)
		{
			RobotMesh->PlayAnimation(DesiredAnimation, true);
		}
	}
}
