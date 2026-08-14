// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VectorActionLockComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorActionLock, Log, All);

bool UVectorActionLockComponent::TryAcquire(UObject* Requester, const FName ActionName)
{
	if (!IsValid(Requester) || ActionName.IsNone())
	{
		return false;
	}
	if (LockOwner.IsValid())
	{
		return LockOwner.Get() == Requester;
	}

	LockOwner = Requester;
	ActiveActionName = ActionName;
	UE_LOG(LogVectorActionLock, Log, TEXT("Action lock acquired: owner=%s action=%s requester=%s"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
		*ActiveActionName.ToString(),
		*Requester->GetName());
	return true;
}

bool UVectorActionLockComponent::Release(UObject* Requester)
{
	if (!Requester || LockOwner.Get() != Requester)
	{
		return false;
	}

	UE_LOG(LogVectorActionLock, Log, TEXT("Action lock released: owner=%s action=%s"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
		*ActiveActionName.ToString());
	LockOwner.Reset();
	ActiveActionName = NAME_None;
	return true;
}

void UVectorActionLockComponent::ForceRelease()
{
	if (LockOwner.IsValid())
	{
		UE_LOG(LogVectorActionLock, Log, TEXT("Action lock force released: owner=%s action=%s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("(none)"),
			*ActiveActionName.ToString());
	}
	LockOwner.Reset();
	ActiveActionName = NAME_None;
}
