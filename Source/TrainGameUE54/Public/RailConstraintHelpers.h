#pragma once
#include "CoreMinimal.h"
#include "RailPhysicsTypes.h"


FORCEINLINE FVector ComputeRailVelocity(
	const FVector& Vel,
    const FVector& Tangent,
    const FRailConstraintProfile& Settings,
    float Dt)
{
	// --- 1. Project velocity onto rail direction
	FVector TangentVel = Tangent * FVector::DotProduct(Vel, Tangent);

	// --- 2. Blend toward constrained velocity
	// lets you soften constraint if desired
	FVector Result = FMath::Lerp(Vel, TangentVel, Settings.AlignmentStrength);

	// --- 3. Gravity projected along rail
	const FVector Gravity(0, 0, -980.f * Settings.GravityScale);
	float Along = FVector::DotProduct(Gravity, Tangent);
	Result += Tangent * Along * Dt;

	// --- 4. Rolling drag
	Result *= (1.f - Settings.Drag * Dt);

	return Result;
}


FORCEINLINE FVector ComputeRailCorrection(
	const FVector& CurrentPos,
    const FVector& RailPos,
    const FRailConstraintProfile& Settings,
    float Dt)
{
	// vector from particle -> rail centerline
	FVector ToRail = RailPos - CurrentPos;

	// soft constraint toward spline
	return ToRail * Settings.StickStrength;
}
