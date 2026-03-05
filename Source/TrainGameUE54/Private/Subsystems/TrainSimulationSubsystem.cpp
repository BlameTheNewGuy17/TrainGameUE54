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
		UE_LOG(LogTemp, Warning, TEXT("Num Bodies: %d"), Input->Bodies.Num());
	}

	// If we are using Spline Following
	else
	{
		
		for (auto& Pair : RollingStockStates)
		{
			FRollingStockID ID = Pair.Key;
			AdvanceRollingStock(ID, DeltaTime);
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
	if (!State || State->bSleeping || State->Bogies.Num() == 0 || !State->Definition) return;

	const float DerailTol = State->Definition->DerailTolerance;

	URailNetworkSubsystem* Rail = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
	if (!Rail) return;

	const float DeltaS = State->Speed * DeltaTime;

	FRailMoveContext Ctx;
	Ctx.bEnforceSignals = false;
	Ctx.bUsePlannedPath = false;

	// ---- Advance ALL bogies independently ----
	for (FBogieState& Bogie : State->Bogies)
	{
		FRailTravelResult Result = Rail->AdvanceAlongRails(Bogie.Location, DeltaS, Ctx);

		if (!Result.bStopped) {
			Bogie.Location.Edge = Result.Edge;
			Bogie.Location.Dir = Result.Dir;
			Bogie.Location.S = Result.S;
		}
		// If stopped, bogie holds its current position this frame
		// Derail validation below will catch if the other bogie keeps stretching away
	}

	// ---- Validate bogie separation ----
	// For now: front vs rear bogie only. Extend to bogie pairs for articulated stock later.
	if (State->Bogies.Num() >= 2)
	{
		const FBogieState& Front = State->Bogies[0];
		const FBogieState& Rear = State->Bogies[1];

		const FVector FrontPos = Rail->GetTransformAtDistance(
			Front.Location.Edge, Front.Location.S).GetLocation();
		const FVector RearPos = Rail->GetTransformAtDistance(
			Rear.Location.Edge, Rear.Location.S).GetLocation();

		const float ActualDist = FVector::Distance(FrontPos, RearPos);
		const float NominalDist = FMath::Abs(Front.OffsetFromCar - Rear.OffsetFromCar);


		if (ActualDist > NominalDist * DerailTol)
		{
			State->bDerailed = true;
			// TODO: broadcast derail event to RailNetwork/visuals
			UE_LOG(LogTemp, Warning, TEXT("Derailed!"));
			UE_LOG(LogTemp, Warning, TEXT("ActualDist=%.1f Nominal=%.1f Threshold=%.1f"),
				ActualDist, NominalDist, NominalDist * DerailTol);
		}
	}
}

