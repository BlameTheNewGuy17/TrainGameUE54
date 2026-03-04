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

FORCEINLINE FVector ComputeRailPositionCorrection(
	const FVector& CurrentPos,
    const FVector& TargetRailPos,
	const FVector& Tangent,
    const FRailConstraintProfile& Settings,
    float Dt)
{
	FVector ToRail = TargetRailPos - CurrentPos;
	//FVector Lateral = ToRail - FVector::DotProduct(ToRail, Tangent) * Tangent;
	//return Lateral * Settings.StickStrength * Dt;

	// soft constraint toward spline
	return ToRail * Settings.StickStrength;
}

FORCEINLINE FVector ComputeRailAngularVelocityCorrection(
	const FQuat& CurrentRot,
	const FQuat& TargetRailRot,
	const FVector& CurrentAngVel,
	const FRailConstraintProfile& Settings,
	float Dt)
{
	FQuat Delta = TargetRailRot * CurrentRot.Inverse();

	FVector Axis;
	float Angle;
	Delta.ToAxisAndAngle(Axis, Angle);

	Angle = FMath::UnwindRadians(Angle);
	Angle = FMath::Clamp(Angle, -Settings.MaxAngleCorrection, Settings.MaxAngleCorrection);

	if (FMath::Abs(Angle) < 1e-4f)
		return FVector::ZeroVector;

	Axis.Normalize();

	Dt = FMath::Max(Dt, 1e-4f);
	FVector ToRail = Axis * (Angle * Settings.AlignmentStrength / Dt);
	FVector Damping = -CurrentAngVel * Settings.AngularDampening;

	return ToRail + Damping;
}