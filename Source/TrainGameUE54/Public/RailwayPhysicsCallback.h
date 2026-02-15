#pragma once

#include "CoreMinimal.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/ParticleHandle.h"

struct FRailSomethingData
{
	FVector Location;
	double MassDotG;
};

struct TRAINGAMEUE54_API FRailwayPhysicsCallbackInput : public Chaos::FSimCallbackInput
{
	int32 TargetID = INDEX_NONE;

	void Reset()
	{
		TargetID = INDEX_NONE;
	}
	
};

class TRAINGAMEUE54_API FRailwayPhysicsCallback	: public Chaos::TSimCallbackObject<
	FRailwayPhysicsCallbackInput,
	Chaos::FSimCallbackNoOutput,
	Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::Presimulate>
{
public:
	FRailwayPhysicsCallback() = default;

	virtual void OnPreIntegrate_Internal() override;
	virtual void OnPreSimulate_Internal() override;
};