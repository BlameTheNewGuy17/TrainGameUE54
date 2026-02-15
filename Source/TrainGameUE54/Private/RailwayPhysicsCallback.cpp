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
				//if (ActiveParticle.UniqueIdx().Idx != Input->TargetID)
				//	continue;

				auto V = ActiveParticle.V();
				V.Z += 50;
				ActiveParticle.SetV(V);
				UE_LOG(LogTemp, Error, TEXT("OnPreIntegrate_Callback: Adding 50 to Active Particle Z Velocity."));
			}
		}
	}
}


void FRailwayPhysicsCallback::OnPreSimulate_Internal()
{

}