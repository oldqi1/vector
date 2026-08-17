// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorCombatTargeting.h"
#include "Combat/VectorTrajectoryPreviewComponent.h"
#include "Gameplay/VectorCharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorCursorTargetPriorityTest,
	"Vector.Combat.Targeting.CursorOverlapPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorCursorTargetPriorityTest::RunTest(const FString& Parameters)
{
	FVectorCursorTargetScore Ground;
	Ground.ScreenDistanceSquared = 0.0;
	Ground.bAirborne = false;
	Ground.SpatialDistanceSquared = 400.0;
	Ground.StableName = TEXT("Ground");

	FVectorCursorTargetScore Airborne = Ground;
	Airborne.ScreenDistanceSquared = FMath::Square(12.0);
	Airborne.bAirborne = true;
	Airborne.StableName = TEXT("Airborne");
	TestTrue(TEXT("Airborne wins when silhouettes overlap"),
		FVectorCombatTargeting::IsCursorCandidatePreferred(Airborne, Ground));

	Airborne.ScreenDistanceSquared = FMath::Square(30.0);
	TestFalse(TEXT("A clearly displaced airborne target does not steal the cursor"),
		FVectorCombatTargeting::IsCursorCandidatePreferred(Airborne, Ground));

	FVectorCursorTargetScore CloserGround = Ground;
	CloserGround.ScreenDistanceSquared = FMath::Square(4.0);
	Ground.ScreenDistanceSquared = FMath::Square(10.0);
	TestTrue(TEXT("Same-state candidates still favor screen proximity"),
		FVectorCombatTargeting::IsCursorCandidatePreferred(CloserGround, Ground));

	FVectorCursorTargetScore Near = Ground;
	Near.ScreenDistanceSquared = Ground.ScreenDistanceSquared;
	Near.SpatialDistanceSquared = 100.0;
	TestTrue(TEXT("Exact screen ties favor the spatially nearer target"),
		FVectorCombatTargeting::IsCursorCandidatePreferred(Near, Ground));

	UVectorCharacterMovementComponent* Movement =
		NewObject<UVectorCharacterMovementComponent>();
	Movement->Velocity = FVector(300.0, 400.0, 700.0);
	FVector PreviewVelocity = FVector::ZeroVector;
	TestTrue(TEXT("Directional preview composition accepts a valid shot"),
		Movement->ComputeDirectionalVelocityOverride(
			FVector::ForwardVector, 1200.0, PreviewVelocity));
	TestTrue(TEXT("Directional preview matches execution composition"),
		PreviewVelocity.Equals(FVector(1200.0, 400.0, 700.0), 1.e-6));
	TestFalse(TEXT("Invalid preview direction is rejected"),
		Movement->ComputeDirectionalVelocityOverride(
			FVector::ZeroVector, 1200.0, PreviewVelocity));
	const double ImpactError =
		UVectorTrajectoryPreviewComponent::ComputeImpactErrorCm(
			FVector(100.0, 200.0, 300.0), FVector(172.0, 296.0, 300.0));
	TestEqual(TEXT("Impact error uses full 3D distance"), ImpactError, 120.0, 1.e-6);
	TestTrue(TEXT("120 cm inclusive verification gate passes"),
		UVectorTrajectoryPreviewComponent::IsImpactWithinTolerance(ImpactError, 120.0));
	TestFalse(TEXT("Error above the gate fails"),
		UVectorTrajectoryPreviewComponent::IsImpactWithinTolerance(120.01, 120.0));
	return true;
}

#endif
