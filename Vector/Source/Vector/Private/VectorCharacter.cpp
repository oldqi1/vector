// Copyright Epic Games, Inc. All Rights Reserved.

#include "VectorCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Combat/VectorImpulseHammerComponent.h"
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
#include "UObject/ConstructorHelpers.h"

AVectorCharacter::AVectorCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVectorCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// 俯视角移动游戏：角色朝向跟随移动方向（WASD），鼠标只控制镜头 Yaw 与瞄准方向。
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

	// 冲量锤装备（S02）：左键按住蓄力、松开释放水平冲量。
	ImpulseHammer = CreateDefaultSubobject<UVectorImpulseHammerComponent>(TEXT("ImpulseHammer"));
}

void AVectorCharacter::BeginPlay()
{
	Super::BeginPlay();

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
}

void AVectorCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRobotAnimation();
}

void AVectorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureRuntimeInput();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AVectorCharacter::HandleMoveInput);
		EnhancedInput->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AVectorCharacter::HandleLookInput);
		EnhancedInput->BindAction(AttackInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleAttackPressed);
		EnhancedInput->BindAction(AttackInputAction, ETriggerEvent::Completed, this, &AVectorCharacter::HandleAttackReleased);
		EnhancedInput->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AVectorCharacter::HandleJumpPressed);
		EnhancedInput->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AVectorCharacter::HandleJumpReleased);
		EnhancedInput->BindAction(ZoomInputAction, ETriggerEvent::Triggered, this, &AVectorCharacter::HandleZoomInput);
	}
}

void AVectorCharacter::EnsureRuntimeInput()
{
	if (MoveInputAction && LookInputAction && AttackInputAction && JumpInputAction && ZoomInputAction && MoveInputMappingContext)
	{
		return;
	}

	MoveInputAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveInputAction->ValueType = EInputActionValueType::Axis2D;

	LookInputAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookInputAction->ValueType = EInputActionValueType::Axis2D;

	AttackInputAction = NewObject<UInputAction>(this, TEXT("IA_Attack"));
	AttackInputAction->ValueType = EInputActionValueType::Boolean;

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

	// 左键：冲量锤蓄力/释放。
	MoveInputMappingContext->MapKey(AttackInputAction, EKeys::LeftMouseButton);

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
	if (LookInput.IsNearlyZero() || !GetController())
	{
		return;
	}

	// 有限水平旋转：只消费 X（Yaw）；Pitch 恒为固定俯角，不响应鼠标 Y。
	AddControllerYawInput(LookInput.X);
}

void AVectorCharacter::HandleAttackPressed()
{
	if (ImpulseHammer)
	{
		ImpulseHammer->StartCharge();
	}
}

void AVectorCharacter::HandleAttackReleased()
{
	if (ImpulseHammer)
	{
		ImpulseHammer->ReleaseCharge();
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
