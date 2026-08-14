// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VectorActionLockComponent.generated.h"

/**
 * 玩家级装备动作互斥锁。
 *
 * 装备仍各自持有共享 FVectorActionTimeline，但开始动作前必须先取得本锁；锁一直保持到
 * Recovery 结束。由请求对象身份而不是字符串负责释放，防止一个装备误释放另一个装备。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VECTOR_API UVectorActionLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 空闲时取得锁；同一请求者重复取得视为成功且不改变动作名。 */
	bool TryAcquire(UObject* Requester, FName ActionName);

	/** 仅当前持有者可以释放。 */
	bool Release(UObject* Requester);

	/** 死亡/重生等硬重置入口。 */
	void ForceRelease();

	UFUNCTION(BlueprintPure, Category = "Vector|Action")
	bool IsLocked() const { return LockOwner.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Vector|Action")
	FName GetActiveActionName() const { return IsLocked() ? ActiveActionName : NAME_None; }

	bool IsOwnedBy(const UObject* Requester) const { return LockOwner.Get() == Requester; }

private:
	TWeakObjectPtr<UObject> LockOwner;
	FName ActiveActionName = NAME_None;
};
