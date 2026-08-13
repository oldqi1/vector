// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorTestDummy.h"

#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stability/VectorStabilityComponent.h"
#include "UObject/ConstructorHelpers.h"

AVectorTestDummy::AVectorTestDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVectorCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// 无 Controller 的站桩靶也要跑移动物理（UE 默认 false 会在 PhysWalking 开头
	// 清空 Velocity 并早退，导致受控冲量永远不被消费）。
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	// 灰盒靶子：动量模型滑行参数（精确解验证，UE 制动 = 指数摩擦 ×v + 线性制动）：
	// Friction = GroundFriction(1.0) × BrakingFrictionFactor(0.5) = 0.5，
	// 制动加速度 a(v) = 0.5v + 400。配合冲量 I=1750 / 质量 1.25/2.5/5.0：
	//   轻 Δv1400 → ~12m（弹药飞出） / 中 Δv700 → ~4m / 重 Δv350 → ~1.2m（推不动）
	GetCharacterMovement()->MaxWalkSpeed = 3000.0;
	GetCharacterMovement()->GroundFriction = 1.0f;
	GetCharacterMovement()->BrakingFrictionFactor = 0.5f;
	GetCharacterMovement()->BrakingDecelerationWalking = 400.0f;

	StabilityComponent = CreateDefaultSubobject<UVectorStabilityComponent>(TEXT("Stability"));
	StabilityComponent->MassClass = MassClass;

	ImpactCollisionComponent = CreateDefaultSubobject<UVectorImpactCollisionComponent>(TEXT("ImpactCollision"));

	HealthComponent = CreateDefaultSubobject<UVectorHealthComponent>(TEXT("Health"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	BodyMesh->SetRelativeScale3D(FVector(0.9f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMeshFinder.Object);
	}

	// 灰盒测试靶不响应输入、不被相机控制。
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;
}

void AVectorTestDummy::BeginPlay()
{
	Super::BeginPlay();

	if (StabilityComponent)
	{
		StabilityComponent->MassClass = MassClass;
	}
	ApplyMassPresentation();
}

void AVectorTestDummy::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateStaggerPresentation();
}

void AVectorTestDummy::UpdateStaggerPresentation()
{
	if (!BodyMesh || !StabilityComponent)
	{
		return;
	}

	// 稳定度状态 → 表现（灰盒期用旋转+颜色表达失衡/倒地）：
	// Stable/Rising = 直立原色；Unbalanced = 白色闪亮；Downed = 躺平（绕 X 旋转 90°）+ 暗色。
	const EVectorStabilityState State = StabilityComponent->GetState();

	if (State == EVectorStabilityState::Downed)
	{
		BodyMesh->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
		UMaterialInstanceDynamic* Material = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), BaseBodyColor * 0.45f);
		}
	}
	else
	{
		BodyMesh->SetRelativeRotation(FRotator::ZeroRotator);
		UMaterialInstanceDynamic* Material = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (Material)
		{
			// 失衡（Unbalanced）短暂白闪，可读"现在可以推"。
			const FLinearColor TargetColor = (State == EVectorStabilityState::Unbalanced)
				? FLinearColor::White
				: BaseBodyColor;
			Material->SetVectorParameterValue(TEXT("Color"), TargetColor);
		}
	}
}

void AVectorTestDummy::ApplyMassPresentation()
{
	if (!BodyMesh)
	{
		return;
	}

	// 质量档 → 颜色与尺寸（轻=绿小 / 中=橙中 / 重=紫大），一眼分辨质量。
	FLinearColor Color = FLinearColor::Green;
	FVector Scale(0.7f);
	switch (MassClass)
	{
	case EVectorMassClass::Light:
		Color = FLinearColor(0.2f, 0.8f, 0.3f);
		Scale = FVector(0.6f);
		break;
	case EVectorMassClass::Medium:
		Color = FLinearColor(1.0f, 0.6f, 0.1f);
		Scale = FVector(0.9f);
		break;
	case EVectorMassClass::Heavy:
		Color = FLinearColor(0.55f, 0.4f, 0.9f);
		Scale = FVector(1.3f);
		break;
	default:
		break;
	}

	BodyMesh->SetRelativeScale3D(Scale);

	// Cube 原点在网格中心：把方块中心抬到"地面以上自身高度一半"，
	// 使方块底部正好落在胶囊底部（= 地面），避免半埋入地。
	const double BodyHalfHeightCm = 50.0 * Scale.Z;
	BodyMesh->SetRelativeLocation(
		FVector(0.0, 0.0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + BodyHalfHeightCm));

	BaseBodyColor = Color;
	UMaterialInstanceDynamic* Material = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
