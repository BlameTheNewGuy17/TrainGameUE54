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

void UTrainSimulationSubsystem::Tick(float DeltaTime)
{

	// Right now we only store a single callback handle pointer. 
	// We do a simple GetAllActorsInWorld type thing, and store the first one that we find that is simulating physics
	// Eventually we'll replace this with the list of currently loaded RollingStockActors. 

    if (!RailCallback) return;

	FPhysicsActorHandle Handle = nullptr;

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        UPrimitiveComponent* Prim = It->FindComponentByClass<UPrimitiveComponent>();
        if (!Prim || !Prim->IsSimulatingPhysics()) continue;

		if (FBodyInstance* BI = Prim->GetBodyInstance())

		Handle = BI->GetPhysicsActorHandle();
		if (Handle)
		{
			auto* Input = RailCallback->GetProducerInputData_External();
			Input->TrackedProxies.Add((void*)Handle);
		}

    }

    if (Handle)
    {
        auto* Input = RailCallback->GetProducerInputData_External();
		Input->TrackedBodies.Add(Handle);
    }
}


void UTrainSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//UE_LOG(LogTemp, Error, TEXT("TrainSimulationSubsystem: World type: %d"), (int32)GetWorld()->WorldType);

}

void UTrainSimulationSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);

	UE_LOG(LogTemp, Warning, TEXT("Registering Chaos callback"));

	if (FPhysScene* Scene = World.GetPhysicsScene())
	{
		if (auto* Solver = Scene->GetSolver())
		{
			RailCallback = Solver->CreateAndRegisterSimCallbackObject_External<FRailwayPhysicsCallback>();
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

			UE_LOG(LogTemp, Warning, TEXT("Bogie S: %f, Offset: %f"), Bogie.Location.S, Bogie.OffsetFromCar);

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

FRollingStockID UTrainSimulationSubsystem::AddRollingStock(ERollingStockType Type, FRailLocation Location)
{
	FRollingStockID NewID;
	NewID.Value = NextRollingStockID++;

	// Set up some defaults
	FRollingStockState State;
	State.ID = NewID;
	State.Speed = 500;
	State.Bogies.Add(FBogieState());
	State.Bogies.Add(FBogieState());

	
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

	UE_LOG(LogTemp, Warning, TEXT("Added rolling stock %d with %d bogies"),
		NewID.Value,
		State.Bogies.Num());

	return NewID;
}

bool UTrainSimulationSubsystem::RemoveRollingStock(FRollingStockID ID)
{ 
	return true;
}

void UTrainSimulationSubsystem::AdvanceRollingStock(FRollingStockID ID, float DeltaTime)
{

	FRollingStockState* State = RollingStockStates.Find(ID);
	if (!State || State->bSleeping || State->Bogies.Num() == 0) return;


	URailNetworkSubsystem* Rail = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
	if (!Rail) return;

	const float DeltaS = State->Speed * DeltaTime;

	// Determine lead bogie
	const bool bAToB = (State->Direction == ERailDirection::AToB);
	const int32 LeadIndex = 0;// bAToB ? 0 : State->Bogies.Num() - 1;

	FBogieState& LeadBogie = State->Bogies[LeadIndex];



	UE_LOG(LogTemp, Warning, TEXT("Num=%d Dir=%d LeadIndex=%d  S0=%f S1=%f"),
		State->Bogies.Num(),
		(int32)State->Direction,
		LeadIndex,
		State->Bogies.IsValidIndex(0) ? State->Bogies[0].Location.S : -999.f,
		State->Bogies.IsValidIndex(1) ? State->Bogies[1].Location.S : -999.f
	);


	// ---- Advance lead bogie ----
	FRailTravelResult LeadResult;
	FRailMoveContext Ctx;
	Ctx.bEnforceSignals = false;
	Ctx.bUsePlannedPath = false;

	LeadResult = Rail->AdvanceAlongRails(LeadBogie.Location, DeltaS, Ctx);
	LeadBogie.Location.Edge = LeadResult.Edge;
	LeadBogie.Location.Dir = LeadResult.Dir;
	LeadBogie.Location.S = LeadResult.S;

	// ---- Resolve trailing bogies by constraint ----
	for (int32 i = 0; i < State->Bogies.Num(); ++i)
	{
		
		if (i == LeadIndex) continue;

		UE_LOG(LogTemp, Warning, TEXT("Updating bogie i=%d (LeadIndex=%d)"), i, LeadIndex);

		FBogieState& Bogie = State->Bogies[i];
		const float Offset = Bogie.OffsetFromCar - LeadBogie.OffsetFromCar;

		FRailTravelResult R = Rail->AdvanceAlongRails(
			LeadBogie.Location,
			-Offset,
			Ctx
		);

		Bogie.Location.Edge = R.Edge;
		Bogie.Location.Dir = R.Dir;
		Bogie.Location.S = R.S;
	}
}


