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

				if (Input->Bodies.Num() == 0) continue;

				int32 FoundIndex = Input->Bodies.IndexOfByPredicate(
					[Proxy](const FTrackedRailBody& B) { return B.Proxy == Proxy; });

				if (FoundIndex == INDEX_NONE) continue;

				const FRailConstraintProfile& Profile = Input->Bodies[FoundIndex].Profile;

				if (!Input->RailNetwork) continue;

		
				FRailLocation TempLoc;
				float DistSq = 0.f;

				// --- 1. Get the particle's REAL physics position (not cached GT position)
				FVector ParticlePos = ActiveParticle.X();

				// --- 2. Project world position -> rail coordinate (Edge + S)
				Input->RailNetwork->FindClosestRailLocation(ParticlePos, TempLoc, DistSq);

				// cache result so GT can read it later
				CachedLoc = TempLoc;
				CachedDistSq = DistSq;
				bFound = true;


				// --- 3. Sample rail transform at that coordinate
				// This gives us the rail's frame of reference at S
				FTransform RailTransform =
					Input->RailNetwork->GetTransformAtDistance(TempLoc.Edge, TempLoc.S);

				FVector RailPos = RailTransform.GetLocation();

				// Forward vector of spline = rail direction
				FVector RailTangent =
					RailTransform.GetRotation().GetForwardVector().GetSafeNormal();


				// --- 4. Read current physics velocity
				FVector Vel = ActiveParticle.V();

				// --- 5. Compute the new velocity
				FVector NewVel = ComputeRailVelocity(
					Vel,
					RailTangent,
					Profile,
					GetDeltaTime_Internal()
				);

				// --- 6. Apply corrected velocity back to particle
				ActiveParticle.SetV(NewVel);


				// --- Constrain position to rail
				FVector Correction = ComputeRailCorrection(
					ActiveParticle.X(),
					RailPos,
					Profile,
					GetDeltaTime_Internal()
				);

				ActiveParticle.SetX(ActiveParticle.X() + Correction);

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
