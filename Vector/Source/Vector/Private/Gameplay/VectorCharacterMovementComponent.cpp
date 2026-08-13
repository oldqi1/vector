// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/VectorCharacterMovementComponent.h"

#include "Combat/VectorImpactCollisionComponent.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorMovement, Log, All);

namespace VectorCharacterMovement
{
	/** 有限向量判定：无 NaN 且各分量有限（冲量注入的硬性安全网）。 */
	bool IsFiniteVector(const FVector& Vector)
	{
		return !Vector.ContainsNaN()
			&& FMath::IsFinite(Vector.X)
			&& FMath::IsFinite(Vector.Y)
			&& FMath::IsFinite(Vector.Z);
	}
}

void UVectorCharacterMovementComponent::StopMovementImmediately()
{
	ClearQueuedWorldVelocityChanges();
	Super::StopMovementImmediately();
}

void UVectorCharacterMovementComponent::ClearQueuedWorldVelocityChanges()
{
	PendingWorldVelocityChange = FVector::ZeroVector;
	AppliedWorldVelocityChangeThisStep = FVector::ZeroVector;
	ActiveVelocityChangeDirection = FVector::ZeroVector;
	bUseVelocityChangeSubsteps = false;
	bIsImpulseDriven = false;
}

bool UVectorCharacterMovementComponent::QueueWorldVelocityChange(
	const FVector& WorldDeltaVelocity)
{
	if (!VectorCharacterMovement::IsFiniteVector(WorldDeltaVelocity))
	{
		return false;
	}

	const FVector CandidateVelocityChange = PendingWorldVelocityChange + WorldDeltaVelocity;
	if (!VectorCharacterMovement::IsFiniteVector(CandidateVelocityChange))
	{
		return false;
	}
	PendingWorldVelocityChange = CandidateVelocityChange;

	// 入队即置位冲量驱动：AI/其他系统立即让路（PauseMove），防止在冲量被
	// CalcVelocity 消费前，路径请求重启等操作清掉 Pending（"第一锤被吞"bug）。
	bIsImpulseDriven = true;
	return true;
}

void UVectorCharacterMovementComponent::CalcVelocity(
	const float DeltaTime,
	const float Friction,
	const bool bFluid,
	const float BrakingDeceleration)
{
	// 每次入口先清零"已应用"事实，只有本次真正注入才写入。
	AppliedWorldVelocityChangeThisStep = FVector::ZeroVector;
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);

	if (!PendingWorldVelocityChange.IsNearlyZero())
	{
		const FVector VelocityChange = PendingWorldVelocityChange;
		PendingWorldVelocityChange = FVector::ZeroVector;
		if (VectorCharacterMovement::IsFiniteVector(Velocity + VelocityChange))
		{
			Velocity += VelocityChange;
			AppliedWorldVelocityChangeThisStep = VelocityChange;
			bIsImpulseDriven = true;
			UE_LOG(LogVectorMovement, Log, TEXT("Impulse consumed: +%.0f cm/s (dir=%s), Velocity=%.0f, Mode=%s"),
				VelocityChange.Size(),
				*VelocityChange.GetSafeNormal().ToCompactString(),
				Velocity.Size(),
				*GetMovementName());
			ActiveVelocityChangeDirection = VelocityChange.GetSafeNormal();
			bUseVelocityChangeSubsteps = !ActiveVelocityChangeDirection.IsNearlyZero();
		}
		return;
	}

	// 冲量已衰减到几乎与初始方向无关（< 30 cm/s 投影）时退出子步模式。
	if (bUseVelocityChangeSubsteps
		&& FVector::DotProduct(Velocity, ActiveVelocityChangeDirection) <= 30.0)
	{
		ActiveVelocityChangeDirection = FVector::ZeroVector;
		bUseVelocityChangeSubsteps = false;
	}

	// 冲量驱动速度衰减到阈值以下：退出"物理运动"状态（不再结算碰撞伤害）。
	if (bIsImpulseDriven && Velocity.Size2D() < ImpulseDrivenMinSpeedCmPerSecond)
	{
		bIsImpulseDriven = false;
	}
}

void UVectorCharacterMovementComponent::HandleImpact(
	const FHitResult& Hit,
	const float TimeSlice,
	const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	// 只有冲量驱动的物理运动才结算碰撞连锁；正常行走/站桩碰撞不产生伤害。
	if (!bIsImpulseDriven || !CharacterOwner)
	{
		return;
	}

	if (UVectorImpactCollisionComponent* ImpactComponent =
		CharacterOwner->FindComponentByClass<UVectorImpactCollisionComponent>())
	{
		ImpactComponent->OnCharacterImpact(Hit, Velocity.Size(), MoveDelta);
	}
}

void UVectorCharacterMovementComponent::OnMovementModeChanged(
	const EMovementMode PreviousMovementMode,
	const uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// 落地（Falling → Walking）：把下落末速度交给碰撞组件结算落地震荡。
	if (PreviousMovementMode == MOVE_Falling && MovementMode == MOVE_Walking)
	{
		if (UVectorImpactCollisionComponent* ImpactComponent =
			CharacterOwner ? CharacterOwner->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
		{
			ImpactComponent->OnLandedWithImpact(Velocity.Z);
		}
	}
}

void UVectorCharacterMovementComponent::PhysWalking(
	const float DeltaTime,
	const int32 Iterations)
{
	if ((!bUseVelocityChangeSubsteps && PendingWorldVelocityChange.IsNearlyZero())
		|| !FMath::IsFinite(DeltaTime)
		|| DeltaTime <= 1.0f / 120.0f)
	{
		Super::PhysWalking(DeltaTime, Iterations);
		return;
	}

	// 高速冲量注入后：把 Walking 拆成 ≤ 1/120 s 的有限子步，避免单帧大位移穿墙。
	constexpr float MaximumVelocityChangeStepSeconds = 1.0f / 120.0f;
	float RemainingSeconds = DeltaTime;
	while (RemainingSeconds > UE_SMALL_NUMBER)
	{
		const float StepSeconds = FMath::Min(
			RemainingSeconds,
			MaximumVelocityChangeStepSeconds);
		Super::PhysWalking(StepSeconds, Iterations);
		RemainingSeconds -= StepSeconds;
		if (MovementMode != MOVE_Walking)
		{
			if (RemainingSeconds > UE_SMALL_NUMBER)
			{
				StartNewPhysics(RemainingSeconds, Iterations + 1);
			}
			break;
		}
	}
}
