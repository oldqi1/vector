// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorActionLockComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorActionLockOwnershipTest,
	"Vector.Action.Lock.Ownership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorActionLockOwnershipTest::RunTest(const FString& Parameters)
{
	UVectorActionLockComponent* Lock = NewObject<UVectorActionLockComponent>();
	// UObject itself is abstract in UE 5.8.  The lock only needs two distinct,
	// valid UObject identities, so concrete transient components are suitable
	// requesters and avoid NewObject<UObject>() triggering an engine ensure.
	UObject* Hammer = NewObject<UVectorActionLockComponent>();
	UObject* Hook = NewObject<UVectorActionLockComponent>();

	TestTrue(TEXT("Hammer acquires idle lock"), Lock->TryAcquire(Hammer, TEXT("ImpulseHammer")));
	TestTrue(TEXT("Lock reports busy"), Lock->IsLocked());
	TestEqual(TEXT("Action name recorded"), Lock->GetActiveActionName(), FName(TEXT("ImpulseHammer")));
	TestTrue(TEXT("Same owner reacquire is idempotent"), Lock->TryAcquire(Hammer, TEXT("ImpulseHammer")));
	TestFalse(TEXT("Hook rejected while hammer owns lock"), Lock->TryAcquire(Hook, TEXT("GravityHook")));
	TestFalse(TEXT("Non-owner cannot release"), Lock->Release(Hook));
	TestTrue(TEXT("Owner releases"), Lock->Release(Hammer));
	TestTrue(TEXT("Hook acquires after release"), Lock->TryAcquire(Hook, TEXT("GravityHook")));
	Lock->ForceRelease();
	TestFalse(TEXT("Force release clears lock"), Lock->IsLocked());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
