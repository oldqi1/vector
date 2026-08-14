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
	// 冲量驱动的"弹药"不响应 AI 停止请求（2026-08-14 根因修复）：
	// AI 的 PathFollowing 每帧更新路径时会调用 StopMovement（AAIController::StopMovement
	// → AbortMove），若此时放行，飞行速度会被瞬间清零——怪物被锤只"顿一下"的根因。
	// 物理运动状态优先：只清输入加速度，保留冲量速度；速度衰减到阈值后
	// bIsImpulseDriven 自动变 false，AI 自然恢复控制。
	if (bIsImpulseDriven)
	{
		Acceleration = FVector::ZeroVector;
		ConsumeInputVector();
		return;
	}

	ClearQueuedWorldVelocityChanges();
	Super::StopMovementImmediately();
}

void UVectorCharacterMovementComponent::ClearQueuedWorldVelocityChanges()
{
	PendingWorldVelocityOverride = FVector::ZeroVector;
	bHasPendingWorldVelocityOverride = false;
	AppliedWorldVelocityChangeThisStep = FVector::ZeroVector;
	ActiveVelocityChangeDirection = FVector::ZeroVector;
	bUseVelocityChangeSubsteps = false;
	bIsImpulseDriven = false;
	MomentumCarrySecondsRemaining = 0.0;
}

bool UVectorCharacterMovementComponent::QueueWorldVelocityOverride(
	const FVector& WorldVelocity)
{
	if (!VectorCharacterMovement::IsFiniteVector(WorldVelocity))
	{
		return false;
	}

	PendingWorldVelocityOverride = WorldVelocity;
	bHasPendingWorldVelocityOverride = true;
	bIsImpulseDriven = true;
	return true;
}

bool UVectorCharacterMovementComponent::QueueDirectionalVelocityOverride(
	const FVector& WorldDirection,
	const double TargetSpeedCmPerSecond)
{
	if (!VectorCharacterMovement::IsFiniteVector(WorldDirection)
		|| !FMath::IsFinite(TargetSpeedCmPerSecond))
	{
		return false;
	}
	const FVector Direction = WorldDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector BaseVelocity = GetEffectiveVelocityForPendingStep();
	const double AlongDirection = FVector::DotProduct(BaseVelocity, Direction);
	const FVector PerpendicularVelocity = BaseVelocity - Direction * AlongDirection;
	return QueueWorldVelocityOverride(PerpendicularVelocity + Direction * TargetSpeedCmPerSecond);
}

void UVectorCharacterMovementComponent::BeginMomentumCarry(const double DurationSeconds)
{
	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0)
	{
		return;
	}
	MomentumCarrySecondsRemaining = FMath::Max(
		MomentumCarrySecondsRemaining, DurationSeconds);
	bIsImpulseDriven = true;
	UE_LOG(LogVectorMovement, Log,
		TEXT("Momentum carry started: owner=%s duration=%.2f velocity=%s"),
		*GetNameSafe(GetOwner()), MomentumCarrySecondsRemaining,
		*GetEffectiveVelocityForPendingStep().ToCompactString());
}

FVector UVectorCharacterMovementComponent::GetEffectiveVelocityForPendingStep() const
{
	return bHasPendingWorldVelocityOverride ? PendingWorldVelocityOverride : Velocity;
}

void UVectorCharacterMovementComponent::CalcVelocity(
	const float DeltaTime,
	const float Friction,
	const bool bFluid,
	const float BrakingDeceleration)
{
	const bool bMomentumCarryActive = MomentumCarrySecondsRemaining > 0.0;
	MomentumCarrySecondsRemaining = FMath::Max(
		0.0, MomentumCarrySecondsRemaining - FMath::Max(0.0f, DeltaTime));
	// 每次入口先清零"已应用"事实，只有本次真正注入才写入。
	AppliedWorldVelocityChangeThisStep = FVector::ZeroVector;

	// 冲量驱动期间免疫 AI/输入加速度（第二层防护，2026-08-14）：
	// RequestDirectMove 拦截只挡住"新请求"，但 PathFollowing 设置的 Acceleration
	// 是成员变量会跨帧残留，Super::CalcVelocity 每帧都会用它把速度拉回追击方向
	// （实测：冲量消费后 Velocity=254 而理论应=1190，差值正是 AI 反向速度 -936）。
	// 清掉 Acceleration 与未消费输入，让被推飞的"弹药"只按冲量速度滑行。
	if (bIsImpulseDriven)
	{
		Acceleration = FVector::ZeroVector;
		ConsumeInputVector();

		// 清除遗留的寻路请求速度（根因，2026-08-14）：
		// AI 让路后不再调用 RequestDirectMove，但 bHasRequestedVelocity 保持 true，
		// ApplyRequestedMove 每帧读到它就把 Velocity 覆盖回追击方向（"Velocity = MoveVelocity"），
		// 冲量被瞬间清掉——移动中怪被锤只"顿一下"的最终根因。
		bHasRequestedVelocity = false;
		RequestedVelocity = FVector::ZeroVector;
	}

	Super::CalcVelocity(
		DeltaTime,
		bMomentumCarryActive
			? static_cast<float>(FMath::Max(0.0, MomentumCarryFriction)) : Friction,
		bFluid,
		bMomentumCarryActive
			? static_cast<float>(FMath::Max(0.0, MomentumCarryBrakingDeceleration))
			: BrakingDeceleration);

	if (bHasPendingWorldVelocityOverride)
	{
		const FVector TargetVelocity = PendingWorldVelocityOverride;
		PendingWorldVelocityOverride = FVector::ZeroVector;
		bHasPendingWorldVelocityOverride = false;
		if (VectorCharacterMovement::IsFiniteVector(TargetVelocity))
		{
			const FVector BeforeOverride = Velocity;
			Velocity = TargetVelocity;
			AppliedWorldVelocityChangeThisStep = Velocity - BeforeOverride;
			bIsImpulseDriven = true;
			UE_LOG(LogVectorMovement, Log, TEXT("Velocity override consumed: before=%s target=%s actual-dv=%s speed=%.0f mode=%s"),
				*BeforeOverride.ToCompactString(),
				*TargetVelocity.ToCompactString(),
				*AppliedWorldVelocityChangeThisStep.ToCompactString(),
				Velocity.Size(),
				*GetMovementName());
			ActiveVelocityChangeDirection = TargetVelocity.GetSafeNormal();
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

	// 地面冲量衰减到阈值以下才退出；升空叉造成的 Falling 在顶点速度短暂接近零时
	// 仍保持物理状态，避免 AI 在半空重新接管。落地后的 Walking 帧再正常退出。
	if (bIsImpulseDriven && !IsFalling() && Velocity.Size() < ImpulseDrivenMinSpeedCmPerSecond)
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

	// 诊断日志（S03 碰撞连锁排查，2026-08-14）：无论是否结算都打印，
	// 用于确认"被推目标是否真的触发了碰撞事件"（紧挨目标可能因初始接触丢失事件）。
	UE_LOG(LogVectorMovement, Verbose, TEXT("HandleImpact: hit=%s impulseDriven=%d speed=%.0f velocity=%s"),
		Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(world/terrain)"),
		bIsImpulseDriven ? 1 : 0,
		Velocity.Size(),
		*Velocity.ToCompactString());

	// 只有冲量驱动的物理运动才结算碰撞连锁；正常行走/站桩碰撞不产生伤害。
	if (!bIsImpulseDriven || !CharacterOwner)
	{
		return;
	}

	if (UVectorImpactCollisionComponent* ImpactComponent =
		CharacterOwner->FindComponentByClass<UVectorImpactCollisionComponent>())
	{
		ImpactComponent->OnCharacterImpact(Hit, MoveDelta);
	}
}

void UVectorCharacterMovementComponent::RequestDirectMove(
	const FVector& MoveVelocity,
	const bool bForceMaxSpeed)
{
	// 冲量驱动期间免疫 AI 移动请求：被推飞的"弹药"不受寻路干扰。
	// AI 的 PathFollowing 每帧调用本函数，若在这里放行，下一帧就会用
	// MoveVelocity 覆盖刚注入的冲量速度（移动中的怪被锤只顿一下的根因）。
	if (bIsImpulseDriven)
	{
		return;
	}
	Super::RequestDirectMove(MoveVelocity, bForceMaxSpeed);
}

void UVectorCharacterMovementComponent::OnMovementModeChanged(
	const EMovementMode PreviousMovementMode,
	const uint8 PreviousCustomMode)
{
	// UE 父类进入 Walking 时会把 Velocity 投影到地面（清掉 Z）；必须先缓存碰前下落速度。
	const double PreLandingVerticalSpeed = Velocity.Z;
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// 落地（Falling → Walking）：把下落末速度交给碰撞组件结算落地震荡。
	if (PreviousMovementMode == MOVE_Falling && MovementMode == MOVE_Walking)
	{
		if (UVectorImpactCollisionComponent* ImpactComponent =
			CharacterOwner ? CharacterOwner->FindComponentByClass<UVectorImpactCollisionComponent>() : nullptr)
		{
			ImpactComponent->OnLandedWithImpact(PreLandingVerticalSpeed);
		}
	}
}

void UVectorCharacterMovementComponent::PhysWalking(
	const float DeltaTime,
	const int32 Iterations)
{
	if ((!bUseVelocityChangeSubsteps && !bHasPendingWorldVelocityOverride)
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
