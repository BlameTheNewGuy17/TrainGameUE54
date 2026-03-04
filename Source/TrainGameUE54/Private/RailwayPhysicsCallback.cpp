#include "RailwayPhysicsCallback.h"
#include "Chaos/DebugDrawQueue.h"
#include "PBDRigidsSolver.h"
#include "Subsystems/RailNetworkSubsystem.h"


using namespace Chaos;


void FRailwayPhysicsCallback::OnPreSimulate_Internal()
{

}

void FRailwayPhysicsCallback::OnPreIntegrate_Internal()
{
	
	// We are running on the PT here...
	if (FPBDRigidsSolver* MySolver = static_cast<FPBDRigidsSolver*>(GetSolver()))
	{
		// Get a reference to the PT input structure
		const FRailwayPhysicsCallbackInput* Input = GetConsumerInput_Internal();
		if (Input)
		{
			CachedVelPre.SetNumZeroed(Input->Bodies.Num());
			CachedVelPost.SetNumZeroed(Input->Bodies.Num());
			CachedImpulse.SetNumZeroed(Input->Bodies.Num());

			// Iterate over all the currenly simulated rigid bodies - They are named Particles in Chaos. 
			TParticleView<FPBDRigidParticles> ActiveParticles = MySolver->GetParticles().GetNonDisabledDynamicView();

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

				if (Input->Bodies[FoundIndex].bDerailed)
					continue;

		
				FRailLocation TempLoc;
				float DistSq = 0.f;

				// --- Get the particle's REAL physics position (not cached GT position)
				FVector ParticlePos = ActiveParticle.X();

				// --- Project world position -> rail coordinate (Edge + S)
				Input->RailNetwork->FindClosestRailLocation(ParticlePos, TempLoc, DistSq);

				// ---  Sample rail transform at that coordinate
				// This gives us the rail's frame of reference at S
				FTransform RailTransform = Input->RailNetwork->GetTransformAtDistance(TempLoc.Edge, TempLoc.S);

				FVector RailPos = RailTransform.GetLocation();

				// Forward vector of spline = rail direction
				FVector RailTangent = RailTransform.GetRotation().GetForwardVector().GetSafeNormal();


				// --- Read current physics velocity, compute new, and apply
				FVector CurrentVel = ActiveParticle.V();
				FVector NewVel = ComputeRailVelocity(CurrentVel, RailTangent, Profile, GetDeltaTime_Internal());
				ActiveParticle.SetV(NewVel);


				// --- Constrain position to rail
				FVector CurrentPos = ActiveParticle.X();
				FVector PosCorrection = ComputeRailPositionCorrection(CurrentPos, RailPos, RailTangent, Profile, GetDeltaTime_Internal());
				ActiveParticle.SetX(ActiveParticle.X() + PosCorrection);

				// --- Constrain the rotation to rail
				FQuat CurrentRot = ActiveParticle.R();
				FVector CurrentAngVel = ActiveParticle.W();
				FVector AngVelCorrection = ComputeRailAngularVelocityCorrection(CurrentRot, RailTransform.GetRotation(), CurrentAngVel, Profile, GetDeltaTime_Internal());
				ActiveParticle.SetW(CurrentAngVel + AngVelCorrection);


				CachedVelPre[FoundIndex] = ActiveParticle.V();

				//UE_LOG(LogTemp, Warning, TEXT("Sleeping: %d"), ActiveParticle.Sleeping());

			}
		}
	}
}

void FRailwayPhysicsCallback::OnPostIntegrate_Internal()
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

				if (Input->Bodies.Num() == 0) continue;

				int32 FoundIndex = Input->Bodies.IndexOfByPredicate(
					[Proxy](const FTrackedRailBody& B) { return B.Proxy == Proxy; });

				if (FoundIndex == INDEX_NONE) continue;

				if (!Input->RailNetwork) continue;

				if (Input->Bodies[FoundIndex].bDerailed)
					continue;

				CachedVelPost[FoundIndex] = ActiveParticle.V();
				CachedImpulse[FoundIndex] = (CachedVelPost[FoundIndex] - CachedVelPre[FoundIndex]).Size();
			}
		}
	}

}

void FRailwayPhysicsCallback::OnPostSolve_Internal()
{
	FRailwayPhysicsCallbackOutput& Output = GetProducerOutputData_Internal();

	// We are running on the PT here...
	if (FPBDRigidsSolver* MySolver = static_cast<FPBDRigidsSolver*>(GetSolver()))
	{
		// Get a reference to the PT input structure
		const FRailwayPhysicsCallbackInput* Input = GetConsumerInput_Internal();
		if (Input)
		{
			Output.Bodies = Input->Bodies;

			// Iterate over all the currenly simulated rigid bodies - They are named Particles in Chaos. 
			TParticleView<FPBDRigidParticles> ActiveParticles = MySolver->GetParticles().GetNonDisabledDynamicView();

			for (auto& ActiveParticle : ActiveParticles)
			{

				// Filter for the actors we care about by comparing raw pointers.
				// It's a litte crude, but I swear to god I could not find any other way to access or verify ID.

				auto* Proxy = (void*)ActiveParticle.PhysicsProxy();

				if (Input->Bodies.Num() == 0) continue;

				int32 FoundIndex = Input->Bodies.IndexOfByPredicate(
					[Proxy](const FTrackedRailBody& B) { return B.Proxy == Proxy; });

				if (FoundIndex == INDEX_NONE) continue;

				if (!Input->RailNetwork) continue;

				if (Input->Bodies[FoundIndex].bDerailed)
					continue;

				FVector tempWorldLoc = ActiveParticle.X();
				FRailLocation tempRailLoc;
				float distSq = 0.f;
				Input->RailNetwork->FindClosestRailLocation(tempWorldLoc, tempRailLoc, distSq);
				FTransform RailTransform = Input->RailNetwork->GetTransformAtDistance(tempRailLoc.Edge, tempRailLoc.S);
				FVector RailRightVector = RailTransform.GetRotation().GetRightVector();


				FVector Vel = ActiveParticle.V();
				float LateralSpeed = FVector::DotProduct(Vel, RailRightVector);
				float AbsLat = FMath::Abs(LateralSpeed);
				UE_LOG(LogTemp, Warning, TEXT("Absolute Lateral Speed for body %d is %f"), FoundIndex, AbsLat);

				float Stress = FMath::Lerp(Input->Bodies[FoundIndex].StressAccumulator, AbsLat, 0.2f);
				Output.Bodies[FoundIndex].StressAccumulator = Stress;
				UE_LOG(LogTemp, Warning, TEXT("Stress for body %d is %f"), FoundIndex, Stress);


				if (Stress > DerailThreshold) {
					Output.Bodies[FoundIndex].bDerailed = true;
					UE_LOG(LogTemp, Error, TEXT("Body %d derailed"), FoundIndex);
				}
			}
		}
	}
}
