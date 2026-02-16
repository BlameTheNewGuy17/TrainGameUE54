#pragma once
#include "CoreMinimal.h"
#include "RailPhysicsTypes.generated.h"

USTRUCT(BlueprintType)
struct FRailConstraintProfile
{
	GENERATED_BODY()

	// How strongly velocity is forced to align to the rail direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AlignmentStrength = 1.0f;

	// Energy loss along the rail (rolling resistance / drag)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Drag = 0.25f;

	// How strongly position is pulled toward spline center
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StickStrength = 0.2f;

	// Multiplier for gravity influence along rail
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GravityScale = 1.0f;
};
