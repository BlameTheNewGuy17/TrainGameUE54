#pragma once

#include "CoreMinimal.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/ParticleHandle.h"
#include "Subsystems/RailNetworkSubsystem.h"

struct FRailSomethingData
{
	FVector Location;
	double MassDotG;
};

struct TRAINGAMEUE54_API FRailwayPhysicsCallbackInput : public Chaos::FSimCallbackInput
{
	URailNetworkSubsystem* RailNetwork = nullptr;

	FRailLocation RailLocation;
	FVector WorldLocation = FVector::ZeroVector;

	TArray<void*> TrackedProxies;

	void Reset()
	{
		RailNetwork = nullptr;
		TrackedProxies.Empty();
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
	float CachedDistSq;

	bool bHasResult = false;

	virtual void OnPreIntegrate_Internal() override;
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPostIntegrate_Internal() override;
};