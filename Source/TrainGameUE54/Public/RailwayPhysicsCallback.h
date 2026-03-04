#pragma once

#include "CoreMinimal.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/ParticleHandle.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "RailConstraintHelpers.h"
#include "RollingStockTypes.h"

struct FTrackedRailBody
{
	TWeakObjectPtr<AActor> Owner;
	void* Proxy;
	FRailConstraintProfile Profile;
	bool bDerailed;
	float StressAccumulator;
	float LastLateralSpeed;
	FRollingStockID ID;
	uint32 Generation; // Increment it when an actor respawns with the same ID. That prevents stale physics outputs from touching new actors after streaming. Not urgent, just a future armor plate.
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
	TArray<FTrackedRailBody> Bodies;

	void Reset()
	{
		Bodies.Empty();
	}
};

class TRAINGAMEUE54_API FRailwayPhysicsCallback	: public Chaos::TSimCallbackObject<
	FRailwayPhysicsCallbackInput,
	FRailwayPhysicsCallbackOutput,
	Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostIntegrate | Chaos::ESimCallbackOptions::PostSolve>
{
public:
	FRailwayPhysicsCallback() = default;

	TArray<FVector> CachedVelPre;
	TArray<FVector> CachedVelPost;
	TArray<float>   CachedImpulse;

	float DerailThreshold = 500.f;

	virtual void OnPreSimulate_Internal() override;
	virtual void OnPreIntegrate_Internal() override;
	virtual void OnPostIntegrate_Internal() override;
	virtual void OnPostSolve_Internal() override;
};