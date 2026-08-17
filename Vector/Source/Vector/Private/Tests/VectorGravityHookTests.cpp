// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/VectorGravityHookMath.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorGravityHookPairMomentumTest,
	"Vector.GravityHook.PairMomentum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorGravityHookPairMomentumTest::RunTest(const FString& Parameters)
{
	FVector VelocityA = FVector::ZeroVector;
	FVector VelocityB = FVector::ZeroVector;
	const bool bSolved = FVectorGravityHookMath::SolveRetractingPairVelocities(
		FVector(-500.0, 0.0, 0.0),
		FVector(500.0, 0.0, 0.0),
		FVector::ZeroVector,
		FVector::ZeroVector,
		1.0,
		5.0,
		900.0,
		0.0,
		1800.0,
		VelocityA,
		VelocityB);
	TestTrue(TEXT("Valid light-heavy pair solves"), bSolved);
	TestEqual(TEXT("Light endpoint carries most inward speed"), VelocityA.X, 750.0, 1.e-6);
	TestEqual(TEXT("Heavy endpoint moves less"), VelocityB.X, -150.0, 1.e-6);
	TestEqual(TEXT("Closing speed matches reel speed"), VelocityA.X - VelocityB.X, 900.0, 1.e-6);
	TestEqual(TEXT("Total planar momentum remains zero"), VelocityA.X + 5.0 * VelocityB.X, 0.0, 1.e-6);

	bool bSpatialRopeTaut = false;
	TestTrue(TEXT("Elevated spatial tether solves"),
		FVectorGravityHookMath::SolveTetheredPairVelocities(
			FVector(-300.0, 0.0, 0.0), FVector(300.0, 0.0, 800.0),
			FVector(0.0, 0.0, -100.0), FVector(0.0, 0.0, 100.0),
			1.0, 2.0, 1000.0, 0.0, 3.0, 1.0 / 60.0,
			FVector::ZeroVector, 1800.0, 1800.0,
			VelocityA, VelocityB, bSpatialRopeTaut));
	TestTrue(TEXT("Height difference contributes to taut cable distance"), bSpatialRopeTaut);
	TestEqual(TEXT("Spatial correction preserves vertical momentum"),
		VelocityA.Z + 2.0 * VelocityB.Z, 100.0, 1.e-6);

	bool bRopeTaut = false;
	TestTrue(TEXT("Slack non-elastic rope solves"),
		FVectorGravityHookMath::SolveTetheredPairVelocities(
			FVector(-400.0, 0.0, 0.0), FVector(400.0, 0.0, 0.0),
			FVector(-100.0, 20.0, 0.0), FVector(100.0, -20.0, 0.0),
			1.0, 1.0, 1000.0, 0.0, 3.0, 1.0 / 60.0,
			FVector::ZeroVector, 1800.0, 1800.0, VelocityA, VelocityB, bRopeTaut));
	TestFalse(TEXT("Rope remains slack below its length"), bRopeTaut);
	TestEqual(TEXT("Slack rope does not alter endpoint A"), VelocityA.X, -100.0, 1.e-6);
	TestEqual(TEXT("Slack rope does not alter endpoint B"), VelocityB.X, 100.0, 1.e-6);

	TestTrue(TEXT("Taut non-elastic rope solves"),
		FVectorGravityHookMath::SolveTetheredPairVelocities(
			FVector(-500.0, 0.0, 0.0), FVector(500.0, 0.0, 0.0),
			FVector(-100.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0),
			1.0, 1.0, 1000.0, 0.0, 3.0, 1.0 / 60.0,
			FVector::ZeroVector, 1800.0, 1800.0, VelocityA, VelocityB, bRopeTaut));
	TestTrue(TEXT("Rope is taut at its maximum length"), bRopeTaut);
	TestEqual(TEXT("Taut rope removes outward separation without rebound"),
		VelocityB.X - VelocityA.X, 0.0, 1.e-6);
	TestEqual(TEXT("Taut correction preserves total momentum"),
		VelocityA.X + VelocityB.X, 0.0, 1.e-6);

	TestTrue(TEXT("Taut rope permits free inward motion"),
		FVectorGravityHookMath::SolveTetheredPairVelocities(
			FVector(-500.0, 0.0, 0.0), FVector(500.0, 0.0, 0.0),
			FVector(100.0, 0.0, 0.0), FVector(-100.0, 0.0, 0.0),
			1.0, 1.0, 1000.0, 0.0, 3.0, 1.0 / 60.0,
			FVector::ZeroVector, 1800.0, 1800.0, VelocityA, VelocityB, bRopeTaut));
	TestEqual(TEXT("Rope never pushes inward-moving endpoints apart"),
		VelocityA.X - VelocityB.X, 200.0, 1.e-6);

	TestTrue(TEXT("Taut winch solves"),
		FVectorGravityHookMath::SolveTetheredPairVelocities(
			FVector(-500.0, 0.0, 0.0), FVector(500.0, 0.0, 0.0),
			FVector::ZeroVector, FVector::ZeroVector,
			1.0, 1.0, 1000.0, 260.0, 3.0, 1.0 / 60.0,
			FVector::ZeroVector, 1800.0, 1800.0, VelocityA, VelocityB, bRopeTaut));
	TestEqual(TEXT("Winch closes taut rope at configured speed"),
		VelocityA.X - VelocityB.X, 260.0, 1.e-6);
	TestEqual(TEXT("Winch preserves total momentum"),
		VelocityA.X + VelocityB.X, 0.0, 1.e-6);

	TestTrue(TEXT("Already faster inward pair solves"),
		FVectorGravityHookMath::SolveRetractingPairVelocities(
			FVector(-500.0, 0.0, 0.0), FVector(500.0, 0.0, 0.0),
			FVector(1000.0, 0.0, 0.0), FVector(-1000.0, 0.0, 0.0),
			1.0, 1.0, 900.0, 0.0, 1800.0, VelocityA, VelocityB));
	TestEqual(TEXT("Reel does not brake stronger existing closing speed"),
		VelocityA.X - VelocityB.X, 2000.0, 1.e-6);

	const FVector CenterOfMass = FVectorGravityHookMath::ComputePlanarCenterOfMass(
		FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0), 1.0, 5.0);
	TestEqual(TEXT("Meeting marker lies near heavy endpoint"), CenterOfMass.X, 2500.0 / 3.0, 1.e-6);

	TestFalse(TEXT("Invalid mass is rejected"),
		FVectorGravityHookMath::SolveRetractingPairVelocities(
			FVector::ZeroVector, FVector(100.0, 0.0, 0.0),
			FVector::ZeroVector, FVector::ZeroVector,
			0.0, 1.0, 900.0, 0.0, 1800.0, VelocityA, VelocityB));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVectorGravityHookAngularMomentumTest,
	"Vector.GravityHook.AngularMomentum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVectorGravityHookAngularMomentumTest::RunTest(const FString& Parameters)
{
	const FVector FarA(-400.0, 0.0, 0.0);
	const FVector FarB(400.0, 0.0, 0.0);
	const FVector InitialVelocityA(0.0, -250.0, 0.0);
	const FVector InitialVelocityB(0.0, 250.0, 0.0);
	const double SpecificAngularMomentum =
		FVectorGravityHookMath::ComputePlanarSpecificAngularMomentum(
			FarA, FarB, InitialVelocityA, InitialVelocityB);
	TestEqual(TEXT("Specific angular momentum is r times relative tangent speed"),
		SpecificAngularMomentum, 400000.0, 1.e-6);

	FVector FarVelocityA = FVector::ZeroVector;
	FVector FarVelocityB = FVector::ZeroVector;
	FVector NearVelocityA = FVector::ZeroVector;
	FVector NearVelocityB = FVector::ZeroVector;
	TestTrue(TEXT("Far pair solves"),
		FVectorGravityHookMath::SolveRetractingPairVelocities(
			FarA, FarB, InitialVelocityA, InitialVelocityB,
			1.0, 1.0, 0.0, SpecificAngularMomentum, 1800.0,
			FarVelocityA, FarVelocityB));
	TestTrue(TEXT("Near pair solves"),
		FVectorGravityHookMath::SolveRetractingPairVelocities(
			FVector(-200.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0),
			InitialVelocityA, InitialVelocityB,
			1.0, 1.0, 0.0, SpecificAngularMomentum, 1800.0,
			NearVelocityA, NearVelocityB));

	const double FarRelativeTangentSpeed = FarVelocityB.Y - FarVelocityA.Y;
	const double NearRelativeTangentSpeed = NearVelocityB.Y - NearVelocityA.Y;
	TestEqual(TEXT("Halving cable length doubles relative tangent speed"),
		NearRelativeTangentSpeed / FarRelativeTangentSpeed, 2.0, 1.e-6);
	const double FarAngularSpeed = FarRelativeTangentSpeed / 800.0;
	const double NearAngularSpeed = NearRelativeTangentSpeed / 400.0;
	TestEqual(TEXT("Halving cable length quadruples angular speed"),
		NearAngularSpeed / FarAngularSpeed, 4.0, 1.e-6);
	TestEqual(TEXT("Pair momentum stays zero while angular speed changes"),
		NearVelocityA.Y + NearVelocityB.Y, 0.0, 1.e-6);

	bool bTetherTaut = false;
	TestTrue(TEXT("Non-elastic tether preserves tangent while taut"),
		FVectorGravityHookMath::SolveTetheredPairVelocities(
			FarA, FarB, InitialVelocityA, InitialVelocityB,
			1.0, 1.0, 800.0, 0.0, 3.0, 1.0 / 60.0,
			FVector(0.0, 0.0, SpecificAngularMomentum), 1800.0, 1800.0,
			FarVelocityA, FarVelocityB, bTetherTaut));
	TestTrue(TEXT("Angular tether is taut"), bTetherTaut);
	TestEqual(TEXT("Taut tether retains angular momentum tangent speed"),
		FarVelocityB.Y - FarVelocityA.Y, 500.0, 1.e-6);

	FVector CappedA = FVector::ZeroVector;
	FVector CappedB = FVector::ZeroVector;
	TestTrue(TEXT("Tangential cap solve succeeds"),
		FVectorGravityHookMath::SolveRetractingPairVelocities(
			FVector(-50.0, 0.0, 0.0), FVector(50.0, 0.0, 0.0),
			FVector::ZeroVector, FVector::ZeroVector,
			1.0, 1.0, 0.0, SpecificAngularMomentum, 900.0,
			CappedA, CappedB));
	TestEqual(TEXT("Relative tangential speed respects safety cap"),
		CappedB.Y - CappedA.Y, 900.0, 1.e-6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
