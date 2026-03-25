// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystems/TrainSimulationSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "RailwayPhysicsCallback.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "EngineUtils.h"


TStatId UTrainSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTrainSimulationSubsystem, STATGROUP_Tickables);
}

void UTrainSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//UE_LOG(LogTemp, Error, TEXT("TrainSimulationSubsystem: World type: %d"), (int32)GetWorld()->WorldType);

}

void UTrainSimulationSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);

	RailNetworkRef = World.GetSubsystem<URailNetworkSubsystem>(); // Store the RailNetworkSubsystem

	// If we are using Chaos Physics
	if (bUsePhysics)
	{
		if (FPhysScene* Scene = World.GetPhysicsScene())
		{
			if (auto* Solver = Scene->GetSolver())
			{
				RailCallback = Solver->CreateAndRegisterSimCallbackObject_External<FRailwayPhysicsCallback>();
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("Registered Chaos callback"));


		// Clear actor refs just to be safe
		PhysActorRefs.Empty();

		// Loop through world and store all Actors using Physics. We'll replace this later with currently loaded Rolling Stock or whatever.
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			UPrimitiveComponent* Prim = It->FindComponentByClass<UPrimitiveComponent>();
			if (!Prim || !Prim->IsSimulatingPhysics()) continue;

			AActor* Actor = *It;

			// All data should be pulled from the actor, but for now we just fill it with blank data
			FRollingStockID ID;
			ID.Value = Actor->GetUniqueID();

			FTrackedRailBody Body;
			Body.ID = ID;
			Body.Owner = Actor; // except this, it's the actor reference, idiot
			Body.Profile = FRailConstraintProfile();
			Body.bDerailed = false;

			RailBodyRegistry.Add(ID, Body);
		}
	}

	// If we are using Spline Following
	else
	{

	}
}

void UTrainSimulationSubsystem::Tick(float DeltaTime)
{
	// If we are using Chaos Physics
	if (bUsePhysics)
	{
		if (!RailCallback) return;

		while (auto OutputHandle = RailCallback->PopOutputData_External())
		{
			const auto* Output = OutputHandle.Get();
			if (!Output) continue;

			for (const FTrackedRailBody& OutBody : Output->Bodies)
			{
				if (FTrackedRailBody* Stored = RailBodyRegistry.Find(OutBody.ID))
				{
					Stored->bDerailed = OutBody.bDerailed;
					Stored->StressAccumulator = OutBody.StressAccumulator;
				}
			}
		}


		auto* Input = RailCallback->GetProducerInputData_External();
		Input->RailNetwork = RailNetworkRef;

		Input->Bodies.Reset();

		for (auto& Pair : RailBodyRegistry)
		{
			FTrackedRailBody& Stored = Pair.Value;

			if (!Stored.Owner.IsValid())
				continue;

			TArray<UPrimitiveComponent*> Comps;
			Stored.Owner->GetComponents<UPrimitiveComponent>(Comps);

			for (UPrimitiveComponent* Comp : Comps)
			{
				if (!Comp->IsSimulatingPhysics())
					continue;

				if (Comp->GetCollisionObjectType() != ECC_Truck)
					continue;

				if (FBodyInstance* BI = Comp->GetBodyInstance())
				{
					FPhysicsActorHandle Handle = BI->GetPhysicsActorHandle();
					if (Handle)
					{
						FTrackedRailBody Copy = Stored;
						Copy.Proxy = (void*)Handle;
						Input->Bodies.Add(Copy);
					}
				}
			}
		}
		if (bLogDebug && bLogMovement) UE_LOG(LogTemp, Warning, TEXT("Num Bodies: %d"), Input->Bodies.Num());
	}

	// If we are using Spline Following
	else
	{
		
		for (auto& Pair : Trains)
		{
			FTrainData& Train = Pair.Value;
			if (!Train.bMoving) continue;

			// Walk from LeadCar following Next, advancing and solving each pair
			AdvanceTrain(Train, DeltaTime);
		}
	}
}

void UTrainSimulationSubsystem::DebugDrawRollingStock(float Duration, float Thickness) const
{
	UWorld* World = GetWorld();
	if (!World) return;

	const URailNetworkSubsystem* Rail = World->GetSubsystem<URailNetworkSubsystem>();
	if (!Rail) return;

	for (const TPair<FRollingStockID, FRollingStockState>& Pair : RollingStockStates)
	{
		const FRollingStockState& State = Pair.Value;

		
		for (const FBogieState& Bogie : State.Bogies)

		{
			const FTransform Xform = Rail->GetTransformAtDistance(Bogie.Location.Edge, Bogie.Location.S);
			const FVector Pos = Xform.GetLocation();
			const FVector Forward = Xform.GetRotation().GetForwardVector();

			if (bLogDebug) UE_LOG(LogTemp, Warning, TEXT("DebugDrawRollingStock: Bogie S: %f, Offset: %f"), Bogie.Location.S, Bogie.OffsetFromCar);

			// Bogie
			DrawDebugSphere(
				World,
				Pos,
				25.f,
				12,
				State.bDerailed ? FColor::Red : FColor::Green,
				false,
				Duration,
				0,
				Thickness
			);

			// Direction indicator
			DrawDebugLine(
				World,
				Pos,
				Pos + Forward * 100.f,
				FColor::White,
				false,
				Duration,
				0,
				Thickness
			);
		}
	}
}

FRollingStockID UTrainSimulationSubsystem::AddRollingStock(URollingStockDefinition* RollingStockDefinition, FRailLocation Location)
{
	FRollingStockID NewID;
	NewID.Value = NextRollingStockID++;

	// Set up some defaults
	FRollingStockState State;
	State.Definition = RollingStockDefinition;
	State.ID = NewID;
	State.Speed = 500;
	State.Bogies.Add(FBogieState());
	State.Bogies.Add(FBogieState());
	State.Direction = ERailDirection::AToB;

	
	constexpr float HalfWheelbase = 450.f;

	// Store offset for later calc
	State.Bogies[0].OffsetFromCar = +HalfWheelbase;
	State.Bogies[1].OffsetFromCar = -HalfWheelbase;

	// Front bogie at location
	State.Bogies[0].Location = Location;

	// Rear bogie is location s - wheelbase
	FRailLocation RearLoc = Location;
	RearLoc.S -= 2.f * HalfWheelbase; // place behind the front bogie
	State.Bogies[1].Location = RearLoc;


	// Add that fucker to the pile
	RollingStockStates.Add(NewID, State);

	if (bLogModification) UE_LOG(LogTemp, Warning, TEXT("Added rolling stock %d with %d bogies"), NewID.Value, State.Bogies.Num());

	return NewID;
}

bool UTrainSimulationSubsystem::RemoveRollingStock(FRollingStockID ID)
{ 
	return true;
}

void UTrainSimulationSubsystem::CoupleCars(FRollingStockID CarA, FRollingStockID CarB, FTrainID& TrainOut)
{
	FRollingStockState* CarAState = RollingStockStates.Find(CarA);
	FRollingStockState* CarBState = RollingStockStates.Find(CarB);

	FTrainID NewTrainID{ NextTrainID++ };
	CarAState->TrainID = NewTrainID;

	if (CarBState) { // if CarBState is invalid, we didn't probably didn't pass a B, so it's a single car train.
		CarBState->TrainID = NewTrainID;
		CarAState->Next = CarB;
		CarBState->Prev = CarA;
	}
	
	if (bLogModification) UE_LOG(LogTemp, Warning, TEXT("CoupleCars: CarA.Next=%d CarB.Prev=%d"), CarAState->Next.Value, CarBState->Prev.Value);

	// Add Train
	FTrainData NewTrainData{ NewTrainID, CarA, CarAState->Direction, true };
	Trains.Add(NewTrainID, NewTrainData);
	TrainOut = NewTrainID;

	if (bLogModification) UE_LOG(LogTemp, Warning, TEXT("Added Train | ID: %d | Lead Car ID: %d | Other Car: %d"), NewTrainID.Value, CarA.Value, CarB.Value);
}

void UTrainSimulationSubsystem::UncoupleCars(FRollingStockID CarA, FRollingStockID CarB, FTrainID& TrainAOut, FTrainID& TrainBOut)
{
}

void UTrainSimulationSubsystem::AdvanceTrain(FTrainData Train, float DeltaTime)
{
	URailNetworkSubsystem* Rail = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
	if (!Rail) return;

	FRollingStockID CarID = Train.LeadCar;
	FRollingStockState* CarState = RollingStockStates.Find(CarID);
	FRailMoveContext Ctx;
	//TArray<FRailLocation> Locations; - Used later for double pass. For now:
	FRailLocation LastSolvedTrailingLoc;

	while (true)
	{
		if (bLogMovement) UE_LOG(LogTemp, Warning, TEXT("AdvanceTrain: Advancing Car | Train %d: Car %d | Next=%d | Prev=%d | Dir=%d"), Train.ID.Value, CarID.Value, CarState->Next.Value, CarState->Prev.Value, (int32)CarState->Direction);
		AdvanceRollingStock(CarID, DeltaTime, LastSolvedTrailingLoc); // Move the car. Returns the solved trailing bogie location.
		CarID = (CarState->Direction == ERailDirection::AToB) ? CarState->Next : CarState->Prev; // get the next car based on what direction we're traveling. Prev and Next is a little ambigious as it shouldn't relate to car travel dir...
		if (!CarID.IsValid()) {
			if (bLogMovement) UE_LOG(LogTemp, Warning, TEXT("AdvanceTrain: End Of Train"));
			break; // no more cars
		}
		
		
		CarState = RollingStockStates.Find(CarID);
		if (!CarState) break; // safety — shouldn't happen but guards against bad linkage
		
		FVector CouplerLocA = Rail->GetTransformAtDistance(LastSolvedTrailingLoc.Edge, LastSolvedTrailingLoc.S).GetLocation();
		// If there IS another car
		Rail->SolveTrailingForLinearDistance( // This *should* provide us with the next approx bogie of the next car
			LastSolvedTrailingLoc, // LastBogieRailLocation
			Rail->GetTransformAtDistance(LastSolvedTrailingLoc.Edge, LastSolvedTrailingLoc.S).GetLocation(), // LastBogieWorldLocation. I have no idea if we need this anchor. I need to look up why Sol thought we needed this.
			100.f, // Fake Coupler Length. In the future, this would ideally be an anchor offset by a little from the bogie, to the coupler, then the 2 couplers combined length, and then another little offset, giving us 3 line segments rather than 1. Anyway...
			Rail->AdvanceAlongRails(LastSolvedTrailingLoc, 100.f, Ctx).RailLoc, // The estimate of the next cars bogie based on that "fake coupler length"
			Ctx, // Context for handling signals and stuff. Might be legacy atp, yet another "consult the Sol" kinda thing. 
			LastSolvedTrailingLoc); // Store the output of that as the start for the next car

		// some debug for this fake coupler length
		FVector CouplerLocB = Rail->GetTransformAtDistance(LastSolvedTrailingLoc.Edge, LastSolvedTrailingLoc.S).GetLocation();
		const float ActualDist = FVector::Distance(CouplerLocA, CouplerLocB);
		if (bLogMovement) UE_LOG(LogTemp, Warning, TEXT("AdvanceTrain: ActualDist=%.1f Nominal=100.0"), ActualDist);
	}
}

void UTrainSimulationSubsystem::AdvanceRollingStock(FRollingStockID ID, float DeltaTime, FRailLocation& SolvedRailLocOut)
{
	FRollingStockState* State = RollingStockStates.Find(ID);
	if (!State || State->bSleeping || State->Bogies.Num() < 2 || !State->Definition.IsValid()) return;

	URailNetworkSubsystem* Rail = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
	if (!Rail) return;

	const float DeltaS = State->Speed * DeltaTime;
	const float NominalWheelbase = FMath::Abs(State->Bogies[0].OffsetFromCar - State->Bogies[1].OffsetFromCar);

	FRailMoveContext Ctx;
	Ctx.bEnforceSignals = false;
	Ctx.bUsePlannedPath = false;

	// ---- Step 1: Determine leading bogie ----
	// AToB = moving forward = positive offset leads
	// BToA = moving backward = negative offset leads
	int32 LeadIdx = -1;
	int32 TrailIdx = -1;
	for (int32 i = 0; i < State->Bogies.Num(); ++i)
	{
		float Offset = State->Bogies[i].OffsetFromCar;
		if (State->Direction == ERailDirection::AToB)
		{
			if (Offset > 0.f) LeadIdx = i;
			else TrailIdx = i;
		}
		else
		{
			if (Offset < 0.f) LeadIdx = i;
			else TrailIdx = i;
		}
	}

	if (LeadIdx == -1 || TrailIdx == -1) return;

	FBogieState& LeadBogie = State->Bogies[LeadIdx];
	FBogieState& TrailBogie = State->Bogies[TrailIdx];

	// ---- Step 2: Advance leading bogie ----
	FRailTravelResult LeadResult = Rail->AdvanceAlongRails(LeadBogie.Location, DeltaS, Ctx);
	if (!LeadResult.bStopped)
		LeadBogie.Location = LeadResult.RailLoc;

	// ---- Step 3: Get leading bogie world position ----
	FVector LeadPos = Rail->GetTransformAtDistance(
		LeadBogie.Location.Edge, LeadBogie.Location.S).GetLocation();

	// ---- Step 4: Solve trailing bogie ----
	FRailLocation SolvedTrailLoc;
	bool bSolved = Rail->SolveTrailingForLinearDistance(
		LeadBogie.Location,
		LeadPos,
		NominalWheelbase,
		TrailBogie.Location,
		Ctx,
		SolvedTrailLoc);

	// ---- Step 5: Collision check stub ----
	// TODO: check if SolvedTrailLoc is occupied

	// ---- Step 6: Store solved location ----
	if (bSolved)
	{
		TrailBogie.Location = SolvedTrailLoc;
		SolvedRailLocOut = SolvedTrailLoc;
	}
	// ---- Derail validation ----
	FVector TrailPos = Rail->GetTransformAtDistance(
		TrailBogie.Location.Edge, TrailBogie.Location.S).GetLocation();

	const float ActualDist = FVector::Distance(LeadPos, TrailPos);
	const float DerailTol = State->Definition->DerailTolerance;

	if (ActualDist > NominalWheelbase * DerailTol)
	{
		State->bDerailed = true;
		if (bLogMovement) UE_LOG(LogTemp, Warning, TEXT("Derailed! ActualDist=%.1f Nominal=%.1f"),
			ActualDist, NominalWheelbase);
	}

	if (bLogMovement) UE_LOG(LogTemp, Warning, TEXT("AdvanceRollingStock: ActualDist=%.1f Nominal=%.1f Threshold=%.1f"),
		ActualDist, NominalWheelbase, NominalWheelbase * DerailTol);
}