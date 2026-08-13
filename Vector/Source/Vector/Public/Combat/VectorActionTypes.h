// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorActionTypes.generated.h"

/**
 * 共享装备动作阶段（红线 DEBT-01 的落地：全项目唯一一套动作阶段枚举）。
 *
 * 冲量锤/引力钩/升空叉/闪避/受击统一走 Idle → Windup(蓄力) → Active(释放)
 * → Recovery(恢复) 四阶段，不再允许每件装备各自定义 Phase 枚举（原 Morphorbit
 * 有 4 套互不兼容的枚举、互斥靠手写 if，此处禁止复制该模式）。
 */
UENUM(BlueprintType)
enum class EVectorActionPhase : uint8
{
	/** 空闲：可发起新动作。 */
	Idle,

	/** 蓄力/前摇：Windup 期间不自动计时结束，由输入释放或取消离开。 */
	Windup,

	/** 释放/命中：固定 ActiveSeconds 后自动进入 Recovery。 */
	Active,

	/** 恢复：固定 RecoverySeconds 后自动回到 Idle。 */
	Recovery,
};

/**
 * 装备动作时间线账本（纯 C++ 结构，无 UObject/World 依赖）。
 *
 * 形态对齐 Morphorbit 后坐力炮时间线（Idle→Charging→Recovery，长帧跨边界只触发
 * 一次），但扩展为四阶段并明确"Windup 是输入驱动的可中断阶段"（蓄力进度由
 * Advance 按 MaxChargeSeconds 增长，实际力度在释放时读取）。运行时与 Automation
 * 共用同一账本。
 */
USTRUCT(BlueprintType)
struct VECTOR_API FVectorActionTimeline
{
	GENERATED_BODY()

	/** 当前阶段。 */
	UPROPERTY(BlueprintReadOnly, Category = "Vector|Action")
	EVectorActionPhase Phase = EVectorActionPhase::Idle;

	/** 蓄力进度 0~1（仅 Windup 阶段由 Advance 增长）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Vector|Action")
	double ChargeProgress = 0.0;

	/** Active/Recovery 剩余时长，单位 s。 */
	UPROPERTY(BlueprintReadOnly, Category = "Vector|Action")
	double ActiveSecondsRemaining = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Vector|Action")
	double RecoverySecondsRemaining = 0.0;

	/** Active 固定时长，单位 s（默认 0.15）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vector|Action", meta = (ClampMin = "0.0", Units = "s"))
	double ActiveSeconds = 0.15;

	/** Recovery 固定时长，单位 s（默认 0.40）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vector|Action", meta = (ClampMin = "0.0", Units = "s"))
	double RecoverySeconds = 0.40;

	/** 蓄满 1.0 所需时长，单位 s（默认 0.80）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vector|Action", meta = (ClampMin = "0.0", Units = "s"))
	double MaxChargeSeconds = 0.80;

	/**
	 * 发起蓄力：仅 Idle 可进入 Windup；重复调用返回 false 且无副作用。
	 */
	bool TryStartWindup()
	{
		if (Phase != EVectorActionPhase::Idle)
		{
			return false;
		}
		Phase = EVectorActionPhase::Windup;
		ChargeProgress = 0.0;
		return true;
	}

	/**
	 * 释放：仅 Windup 可进入 Active（ChargeProgress 同步 clamp 到 0~1）。
	 * 调用方在返回 true 时执行本次释放的实际效果。
	 */
	bool TryRelease()
	{
		if (Phase != EVectorActionPhase::Windup)
		{
			return false;
		}
		Phase = EVectorActionPhase::Active;
		ActiveSecondsRemaining = FMath::Max(0.0, ActiveSeconds);
		ChargeProgress = FMath::Clamp(ChargeProgress, 0.0, 1.0);
		return true;
	}

	/** 取消蓄力：仅 Windup 可回到 Idle（用于被中断/闪避取消）。 */
	bool TryCancel()
	{
		if (Phase != EVectorActionPhase::Windup)
		{
			return false;
		}
		Phase = EVectorActionPhase::Idle;
		ChargeProgress = 0.0;
		return true;
	}

	/**
	 * 按真实帧时间推进：Windup 只增长蓄力进度；Active/Recovery 倒计时并在归零时
	 * 自动迁移到下一阶段。非法 DeltaSeconds 安全返回。
	 */
	void Advance(double DeltaSeconds)
	{
		if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
		{
			return;
		}

		if (Phase == EVectorActionPhase::Windup)
		{
			ChargeProgress = FMath::Clamp(
				ChargeProgress + DeltaSeconds / FMath::Max(UE_SMALL_NUMBER, MaxChargeSeconds),
				0.0,
				1.0);
		}
		else if (Phase == EVectorActionPhase::Active)
		{
			ActiveSecondsRemaining -= DeltaSeconds;
			if (ActiveSecondsRemaining <= 0.0)
			{
				Phase = EVectorActionPhase::Recovery;
				RecoverySecondsRemaining = FMath::Max(0.0, RecoverySeconds);
			}
		}
		else if (Phase == EVectorActionPhase::Recovery)
		{
			RecoverySecondsRemaining -= DeltaSeconds;
			if (RecoverySecondsRemaining <= 0.0)
			{
				Phase = EVectorActionPhase::Idle;
			}
		}
	}

	/** 重置回 Idle 并清零全部计时/蓄力。 */
	void Reset()
	{
		Phase = EVectorActionPhase::Idle;
		ChargeProgress = 0.0;
		ActiveSecondsRemaining = 0.0;
		RecoverySecondsRemaining = 0.0;
	}

	/** 是否处于任何非空闲阶段（忙）。 */
	bool IsBusy() const
	{
		return Phase != EVectorActionPhase::Idle;
	}
};
