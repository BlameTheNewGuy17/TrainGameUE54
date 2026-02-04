// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/TrainSimulationSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "Stats/Stats.h"

TStatId UTrainSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTrainSimulationSubsystem, STATGROUP_Tickables);
}

void UTrainSimulationSubsystem::Tick(float DeltaTime)
{
	for (TPair<FRollingStockID, FRollingStockState>& Pair : RollingStockStates)
	{
		AdvanceRollingStock(Pair.Key, DeltaTime);
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

		const FTransform Xform = Rail->GetTransformAtDistance(State.Location.Edge, State.Location.S);
		const FVector Pos = Xform.GetLocation();
		const FVector Forward = Xform.GetRotation().GetForwardVector();

		// Body
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

FRollingStockID UTrainSimulationSubsystem::AddRollingStock(ERollingStockKind Kind, FRailLocation Location)
{
	FRollingStockID NewID;
	NewID.Value = NextRollingStockID++;

	FRollingStockState State;
	State.ID = NewID;
	State.Speed = 100;
	State.Location = Location;

	RollingStockStates.Add(NewID, State);

	return NewID;
}

bool UTrainSimulationSubsystem::RemoveRollingStock(FRollingStockID ID)
{ 
	return true;
}

void UTrainSimulationSubsystem::AdvanceRollingStock(FRollingStockID ID, float DeltaTime)
{
	FRollingStockState* State = RollingStockStates.Find(ID);
	if (!State) return;

	const float DeltaS = State->Speed * DeltaTime;

	if (URailNetworkSubsystem* Rail = GetWorld()->GetSubsystem<URailNetworkSubsystem>())
	{
		FRailTravelResult Result;
		FRailMoveContext Ctx;
		Ctx.bEnforceSignals = false;
		Ctx.bUsePlannedPath = false;
		Result = Rail->AdvanceAlongRails(State->Location, DeltaS, Ctx);
		State->Location.Edge = Result.Edge;
		State->Location.Dir = Result.Dir;
		State->Location.S = Result.S;
	}
}


