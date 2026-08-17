// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorTestDummy.h"

#include "Combat/VectorHealthComponent.h"
#include "Combat/VectorImpactCollisionComponent.h"
#include "Combat/VectorWallBurstComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Physics/VectorPhysicsModifierComponent.h"
#include "Stability/VectorStabilityComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorPresentation, Log, All);

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
	WallBurstComponent = CreateDefaultSubobject<UVectorWallBurstComponent>(TEXT("WallBurst"));

	HealthComponent = CreateDefaultSubobject<UVectorHealthComponent>(TEXT("Health"));
	PhysicsModifierComponent = CreateDefaultSubobject<UVectorPhysicsModifierComponent>(TEXT("PhysicsModifier"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	BodyMesh->SetRelativeScale3D(FVector(0.9f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);

	// 材质参数在不同引擎版本/基础材质中可能不存在，因此额外挂一个真实点光源，
	// 再配合尺寸脉冲保证预警在 Lit/Unlit 视图下都能被玩家看见。
	AttackWarningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AttackWarningLight"));
	AttackWarningLight->SetupAttachment(GetCapsuleComponent());
	AttackWarningLight->SetRelativeLocation(FVector(0.0, 0.0, 60.0));
	AttackWarningLight->SetLightColor(FLinearColor::White);
	AttackWarningLight->SetIntensity(0.0f);
	AttackWarningLight->SetAttenuationRadius(500.0f);
	AttackWarningLight->SetCastShadows(false);
	AttackWarningLight->SetVisibility(false);

	LiftForkLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LiftForkLight"));
	LiftForkLight->SetupAttachment(GetCapsuleComponent());
	LiftForkLight->SetRelativeLocation(FVector(0.0, 0.0, 110.0));
	LiftForkLight->SetLightColor(FLinearColor(1.0f, 0.82f, 0.05f));
	LiftForkLight->SetIntensity(0.0f);
	LiftForkLight->SetAttenuationRadius(420.0f);
	LiftForkLight->SetVisibility(true);

	ImpactFeedbackLight = CreateDefaultSubobject<UPointLightComponent>(
		TEXT("ImpactFeedbackLight"));
	ImpactFeedbackLight->SetupAttachment(GetCapsuleComponent());
	ImpactFeedbackLight->SetRelativeLocation(FVector(0.0, 0.0, 45.0));
	ImpactFeedbackLight->SetIntensity(0.0f);
	ImpactFeedbackLight->SetAttenuationRadius(360.0f);
	ImpactFeedbackLight->SetCastShadows(false);
	ImpactFeedbackLight->SetVisibility(true);

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
	if (ImpactCollisionComponent)
	{
		ImpactCollisionComponent->OnBodyImpact.AddUObject(
			this, &AVectorTestDummy::HandleBodyImpactFeedback);
		ImpactCollisionComponent->OnSurfaceContact.AddUObject(
			this, &AVectorTestDummy::HandleSurfaceImpactFeedback);
	}
	ApplyMassPresentation();
}

void AVectorTestDummy::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (ImpactFeedbackSecondsRemaining > 0.0 && ImpactFeedbackLight)
	{
		ImpactFeedbackSecondsRemaining = FMath::Max(
			0.0, ImpactFeedbackSecondsRemaining - SafeDeltaSeconds);
		const double Alpha = ImpactFeedbackDurationSeconds > 0.0
			? ImpactFeedbackSecondsRemaining / ImpactFeedbackDurationSeconds : 0.0;
		ImpactFeedbackLight->SetIntensity(static_cast<float>(
			ImpactFeedbackPeakIntensity * FMath::Clamp(Alpha, 0.0, 1.0)));
	}
	if (LiftForkFlashSecondsRemaining > 0.0)
	{
		LiftForkFlashSecondsRemaining = FMath::Max(
			0.0, LiftForkFlashSecondsRemaining - SafeDeltaSeconds);
		const double Ratio = FMath::Clamp(LiftForkFlashSecondsRemaining / 0.45, 0.0, 1.0);
		LiftForkLight->SetIntensity(static_cast<float>(7000.0 * Ratio));
	}
	if (LiftForkTraceSecondsRemaining > 0.0)
	{
		LiftForkTraceSecondsRemaining = FMath::Max(
			0.0, LiftForkTraceSecondsRemaining - SafeDeltaSeconds);
		const double HeightCm = FMath::Max(0.0, GetActorLocation().Z - LiftForkStartLocation.Z);
		LiftForkPeakHeightCm = FMath::Max(LiftForkPeakHeightCm, HeightCm);
		if (LiftForkFlashSecondsRemaining <= 0.0 && LiftForkLight)
		{
			const UCharacterMovementComponent* Movement = GetCharacterMovement();
			LiftForkLight->SetIntensity(Movement && Movement->IsFalling() ? 2600.0f : 0.0f);
		}
		if (GetWorld())
		{
			DrawDebugLine(GetWorld(), LiftForkStartLocation, GetActorLocation(),
				FColor::Yellow, false, 0.05f, 0, 5.0f);
			DrawDebugSphere(GetWorld(), GetActorLocation(), 24.0f, 12,
				FColor::Yellow, false, 0.05f, 0, 3.0f);
		}

		if (!bLiftForkDiagnosticSampled)
		{
			LiftForkDiagnosticDelaySecondsRemaining -= SafeDeltaSeconds;
			if (LiftForkDiagnosticDelaySecondsRemaining <= 0.0)
			{
				bLiftForkDiagnosticSampled = true;
				const UCharacterMovementComponent* Movement = GetCharacterMovement();
				const double VerticalVelocity = Movement ? Movement->Velocity.Z : 0.0;
				const bool bLiftVerified = HeightCm >= 25.0 || VerticalVelocity >= 100.0;
				UE_LOG(LogVectorPresentation, Log,
					TEXT("Lift verification: actor=%s deltaZ=%.1f peakZ=%.1f velocityZ=%.1f mode=%s check=%s"),
					*GetName(), HeightCm, LiftForkPeakHeightCm, VerticalVelocity,
					Movement ? *Movement->GetMovementName() : TEXT("(none)"),
					bLiftVerified ? TEXT("PASS") : TEXT("FAIL"));
			}
		}
	}
	UpdateStaggerPresentation();
}

void AVectorTestDummy::HandleBodyImpactFeedback(AActor* OtherActor)
{
	if (!OtherActor)
	{
		return;
	}
	const UVectorCharacterMovementComponent* Movement =
		FindComponentByClass<UVectorCharacterMovementComponent>();
	TriggerImpactFeedback(Movement
		? Movement->GetEffectiveVelocityForPendingStep().Size()
		: GetVelocity().Size());
}

void AVectorTestDummy::HandleSurfaceImpactFeedback(
	const double ClosingSpeedCmPerSecond)
{
	TriggerImpactFeedback(ClosingSpeedCmPerSecond);
}

void AVectorTestDummy::TriggerImpactFeedback(const double ImpactSpeedCmPerSecond)
{
	if (!ImpactFeedbackLight || !FMath::IsFinite(ImpactSpeedCmPerSecond)
		|| ImpactSpeedCmPerSecond < 300.0)
	{
		return;
	}
	FLinearColor PulseColor(0.1f, 0.85f, 1.0f);
	ImpactFeedbackDurationSeconds = 0.09;
	ImpactFeedbackPeakIntensity = 3200.0;
	if (ImpactSpeedCmPerSecond >= 1200.0)
	{
		PulseColor = FLinearColor(1.0f, 0.12f, 0.7f);
		ImpactFeedbackDurationSeconds = 0.18;
		ImpactFeedbackPeakIntensity = 10500.0;
	}
	else if (ImpactSpeedCmPerSecond >= 700.0)
	{
		PulseColor = FLinearColor(1.0f, 0.72f, 0.05f);
		ImpactFeedbackDurationSeconds = 0.13;
		ImpactFeedbackPeakIntensity = 6500.0;
	}
	ImpactFeedbackSecondsRemaining = ImpactFeedbackDurationSeconds;
	ImpactFeedbackLight->SetLightColor(PulseColor);
	ImpactFeedbackLight->SetIntensity(static_cast<float>(ImpactFeedbackPeakIntensity));
}

void AVectorTestDummy::TriggerLiftForkPresentation()
{
	LiftForkFlashSecondsRemaining = 0.45;
	LiftForkTraceSecondsRemaining = 4.0;
	LiftForkDiagnosticDelaySecondsRemaining = 0.12;
	LiftForkStartLocation = GetActorLocation();
	LiftForkPeakHeightCm = 0.0;
	bLiftForkDiagnosticSampled = false;
	if (LiftForkLight)
	{
		LiftForkLight->SetIntensity(7000.0f);
	}
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
	const double LiftHeightCm = LiftForkTraceSecondsRemaining > 0.0
		? FMath::Max(0.0, GetActorLocation().Z - LiftForkStartLocation.Z)
		: 0.0;
	const float LiftReadabilityScale = 1.0f + 0.30f * static_cast<float>(
		FMath::Clamp(LiftHeightCm / 250.0, 0.0, 1.0));
	float WarningPulseAlpha = 0.0f;
	if (bAttackWarningActive)
	{
		const double TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		WarningPulseAlpha = 0.5f + 0.5f * FMath::Sin(static_cast<float>(TimeSeconds * UE_TWO_PI * 8.0));
		BodyMesh->SetRelativeScale3D(BaseBodyScale
			* FMath::Max(LiftReadabilityScale, 1.08f + 0.12f * WarningPulseAlpha));
	}
	else
	{
		BodyMesh->SetRelativeScale3D(BaseBodyScale * LiftReadabilityScale);
	}
	if (AttackWarningLight)
	{
		const bool bModifierVisible = bLubricatedPresentation || bBuoyantPresentation;
		AttackWarningLight->SetVisibility(bAttackWarningActive || bModifierVisible);
		if (bAttackWarningActive)
		{
			AttackWarningLight->SetLightColor(FLinearColor::White);
			AttackWarningLight->SetIntensity(9000.0f + 9000.0f * WarningPulseAlpha);
		}
		else if (bBuoyantPresentation)
		{
			AttackWarningLight->SetLightColor(FLinearColor(0.15f, 0.85f, 1.0f));
			AttackWarningLight->SetIntensity(bLubricatedPresentation ? 6500.0f : 5000.0f);
		}
		else if (bLubricatedPresentation)
		{
			AttackWarningLight->SetLightColor(FLinearColor(0.08f, 0.25f, 1.0f));
			AttackWarningLight->SetIntensity(4500.0f);
		}
		else
		{
			AttackWarningLight->SetIntensity(0.0f);
		}
	}

	if (State == EVectorStabilityState::Downed)
	{
		BodyMesh->SetRelativeRotation(BaseBodyRotation + FRotator(90.0, 0.0, 0.0));
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("Color"), BaseBodyColor * 0.45f);
		}
	}
	else
	{
		BodyMesh->SetRelativeRotation(BaseBodyRotation);
		if (BodyMaterial)
		{
			// 攻击预警或失衡时白闪；结束后准确恢复当前质量档原色。
			FLinearColor TargetColor = BaseBodyColor;
			if (bLubricatedPresentation && bBuoyantPresentation)
			{
				TargetColor = FLinearColor(0.1f, 1.0f, 1.0f);
			}
			else if (bBuoyantPresentation)
			{
				TargetColor = FLinearColor(0.2f, 0.85f, 1.0f);
			}
			else if (bLubricatedPresentation)
			{
				TargetColor = FLinearColor(0.08f, 0.25f, 1.0f);
			}
			if (bAttackWarningActive || State == EVectorStabilityState::Unbalanced)
			{
				TargetColor = FLinearColor::White;
			}
			BodyMaterial->SetVectorParameterValue(TEXT("Color"), TargetColor);
		}
	}
}

void AVectorTestDummy::SetPhysicsModifierPresentation(
	const bool bLubricated,
	const bool bBuoyant)
{
	bLubricatedPresentation = bLubricated;
	bBuoyantPresentation = bBuoyant;
	UpdateStaggerPresentation();
}

void AVectorTestDummy::SetAttackWarningPresentation(const bool bActive)
{
	if (bAttackWarningActive == bActive)
	{
		return;
	}
	bAttackWarningActive = bActive;
	UpdateStaggerPresentation();
	UE_LOG(LogVectorPresentation, Log, TEXT("Attack warning visual: actor=%s active=%s"),
		*GetName(), bActive ? TEXT("YES") : TEXT("no"));
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

	BaseBodyScale = Scale;
	BaseBodyRotation = FRotator::ZeroRotator;
	BodyMesh->SetRelativeScale3D(BaseBodyScale);
	BodyMesh->SetRelativeRotation(BaseBodyRotation);

	// Cube 原点在网格中心：把方块中心抬到"地面以上自身高度一半"，
	// 使方块底部正好落在胶囊底部（= 地面），避免半埋入地。
	const double BodyHalfHeightCm = 50.0 * Scale.Z;
	BodyMesh->SetRelativeLocation(
		FVector(0.0, 0.0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + BodyHalfHeightCm));

	BaseBodyColor = Color;
	if (!BodyMaterial)
	{
		BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
