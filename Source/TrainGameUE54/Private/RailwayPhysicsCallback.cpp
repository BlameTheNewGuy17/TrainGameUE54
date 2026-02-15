#include "RailwayPhysicsCallback.h"
#include "Chaos/DebugDrawQueue.h"
#include "PBDRigidsSolver.h"

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

			for (auto& ActiveParticle : ActiveParticles)
			{
				
				// Filter for the actors we care about by comparing raw pointers.
				// It's a litte crude, but I swear to god I could not find any other way to access or verify ID.

				auto* Proxy = (void*)ActiveParticle.PhysicsProxy();
				if (Input->TrackedProxies.Num() == 0) return;
				if (Proxy != Input->TrackedProxies[0]) continue;


				// Little test code. Takes whatever handle we have stored, and make it fly.
				auto V = ActiveParticle.V();
				V.Z += 50;
				ActiveParticle.SetV(V);
				//UE_LOG(LogTemp, Error, TEXT("OnPreIntegrate_Callback: Adding 50 to Active Particle Z Velocity."));
			}
		}
	}
}


void FRailwayPhysicsCallback::OnPreSimulate_Internal()
{

}