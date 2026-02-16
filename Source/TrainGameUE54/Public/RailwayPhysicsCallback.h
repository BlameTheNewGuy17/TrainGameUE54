#pragma once

#include "CoreMinimal.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/ParticleHandle.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "RailConstraintHelpers.h"

struct FTrackedRailBody
{
	void* Proxy;
	FRailConstraintProfile Profile;
};

struct TRAINGAMEUE54_API FRailwayPhysicsCallbackInput : public Chaos::FSimCallbackInput
{
	URailNetworkSubsystem* RailNetwork = nullptr;
	TArray<FTrackedRailBody> Bodies;

	void Reset()
	{
		RailNetwork = nullptr;
		Bodies.Empty();
	}
};

struct TRAINGAMEUE54_API FRailwayPhysicsCallbackOutput : public Chaos::FSimCallbackOutput
{
	FRailLocation RailLocation;
	float DistSq;

	void Reset()
	{ }
};

class TRAINGAMEUE54_API FRailwayPhysicsCallback	: public Chaos::TSimCallbackObject<
	FRailwayPhysicsCallbackInput,
	FRailwayPhysicsCallbackOutput,
	Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostIntegrate>
{
public:
	FRailwayPhysicsCallback() = default;

	FRailLocation CachedLoc;
	float CachedDistSq = 0.f;

	bool bHasResult = false;

	virtual void OnPreIntegrate_Internal() override;
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPostIntegrate_Internal() override;
};