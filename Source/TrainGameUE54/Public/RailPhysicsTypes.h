#pragma once
#include "CoreMinimal.h"
#include "RailPhysicsTypes.generated.h"


constexpr ECollisionChannel ECC_Body  = ECC_GameTraceChannel1;
constexpr ECollisionChannel ECC_Truck = ECC_GameTraceChannel2;
constexpr ECollisionChannel ECC_Wheel = ECC_GameTraceChannel3;
constexpr ECollisionChannel ECC_Rail  = ECC_GameTraceChannel4;

USTRUCT(BlueprintType)
struct FRailConstraintProfile
{
	GENERATED_BODY()

	// How strongly velocity is forced to align to the rail direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AlignmentStrength = 0.15f;

	// How far we can correct the angle
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxAngleCorrection = PI * 0.25f; // 45 degrees

	// 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngularDampening = 2.0f;

	// Energy loss along the rail (rolling resistance / drag)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Drag = 0.05f;

	// How strongly position is pulled toward spline center
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StickStrength = 0.2f;

	// Multiplier for gravity influence along rail
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GravityScale = 1.0f;
};
