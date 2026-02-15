#include "RailwayPhysicsCallback.h"
#include "Chaos/DebugDrawQueue.h"
#include "PBDRigidsSolver.h"
#include "Subsystems/RailNetworkSubsystem.h"


using namespace Chaos;

void FRailwayPhysicsCallback::OnPreIntegrate_Internal()
{
	
	// We are running on the PT here...
	if (FPBDRigidsSolver* MySolver = static_cast<FPBDRigidsSolver*>(GetSolver()))
	{
		// Get a reference to the PT input structure
		const FRailwayPhysicsCallbackInput* Input = GetConsumerInput_Internal();
		if (Input)
		{
			// Iterate over all the currenly simulated rigid bodies - They are named Particles in Chaos. 
			TParticleView<FPBDRigidParticles> ActiveParticles = MySolver->GetParticles().GetNonDisabledDynamicView();

			bool bFound = false;

			for (auto& ActiveParticle : ActiveParticles)
			{
				
				// Filter for the actors we care about by comparing raw pointers.
				// It's a litte crude, but I swear to god I could not find any other way to access or verify ID.

				auto* Proxy = (void*)ActiveParticle.PhysicsProxy();
				if (Input->TrackedProxies.Num() == 0) continue;
				if (Proxy != Input->TrackedProxies[0]) continue;

				if (!Input->RailNetwork) continue;

				FRailLocation TempLoc;
				float DistSq = 0.f;
				FVector ParticlePos = ActiveParticle.X();

				Input->RailNetwork->FindClosestRailLocation(ParticlePos, TempLoc, DistSq);

				CachedLoc = TempLoc;
				CachedDistSq = DistSq;
				bFound = true;

				FVector RailPos = Input->RailNetwork
					->GetTransformAtDistance(TempLoc.Edge, TempLoc.S)
					.GetLocation();

				FVector RailTangent = Input->RailNetwork
					->GetTransformAtDistance(TempLoc.Edge, TempLoc.S)
					.GetRotation()
					.GetForwardVector()
					.GetSafeNormal();

				FVector Vel = ActiveParticle.V();

				FVector NewVel = RailTangent * FVector::DotProduct(Vel, RailTangent);

				ActiveParticle.SetV(NewVel);

				FVector ToRail = RailPos - ActiveParticle.X();
				ActiveParticle.SetX(ActiveParticle.X() + ToRail * 0.2f);

			}
			bHasResult = bFound;
		}
	}
}


void FRailwayPhysicsCallback::OnPreSimulate_Internal()
{

}

void FRailwayPhysicsCallback::OnPostIntegrate_Internal()
{
	if (!bHasResult)
		return;
	FRailwayPhysicsCallbackOutput& Out = GetProducerOutputData_Internal();
	Out.RailLocation = CachedLoc;
	Out.DistSq = CachedDistSq;

}
