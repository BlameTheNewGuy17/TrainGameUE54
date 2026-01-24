// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/RailNetworkSubsystem.h"
#include "RailMath.h"
#include "DrawDebugHelpers.h"

/*
URailNetworkSubsystem
Implementation skeleton for the rail simulation core.
This file intentionally contains minimal logic so the project links and you can
incrementally fill in behavior starting with GetTransformAtDistance.
*/

// ---------- CORE GRAPH ----------

void URailNetworkSubsystem::DebugDrawRailNetwork(float Duration, float Thickness) const
{
	UWorld* World = GetWorld();

	// ---- Draw Nodes ----
	for (const auto& Pair : Nodes)
	{
		const FRailNodeData & Node = Pair.Value;

		DrawDebugSphere(
			World,
			Node.WorldPosition,
			20.f,
			12,
			FColor::Yellow,
			false,
			Duration,
			0, 
			2.f);
	}

	// ---- Draw Edges (sampled Hermite curves) ----
	const int32 NumSegments = 32; // increase for smoother curves

	for (const auto& Pair : Edges)
	{
		const FRailEdgeData& Edge = Pair.Value;

		const FRailNodeData* NA = Nodes.Find(Edge.NodeA.Value);
		const FRailNodeData* NB = Nodes.Find(Edge.NodeB.Value);
		if (!NA || !NB) continue;

		FVector PrevPos = NA->WorldPosition;

		for (int32 i = 1; i <= NumSegments; ++i)
		{
			const float T = (float)i / (float)NumSegments;

			const FVector Pos = RailMath::EvalHermitePos(
				NA->WorldPosition, Edge.TangentA,
				NB->WorldPosition, Edge.TangentB,
				T);

			DrawDebugLine(
				World,
				PrevPos,
				Pos,
				FColor::Cyan,
				false,
				Duration,
				0,
				Thickness);

			PrevPos = Pos;
		}

		// ---- Optional: draw tangents at endpoints ----
		DrawDebugLine(
			World,
			NA->WorldPosition,
			NA->WorldPosition + Edge.TangentA,
			FColor::Green,
			false,
			Duration,
			0,
			2.f);

		DrawDebugLine(
			World,
			NB->WorldPosition,
			NB->WorldPosition + Edge.TangentB,
			FColor::Red,
			false,
			Duration,
			0,
			2.f);
	}
}

FRailNodeID URailNetworkSubsystem::CreateNode(const FVector& WorldPos, ERailNodeType Type, FRailEdgeID ActiveEdge)
{
	FRailNodeID NewID;
	NewID.Value = NextNodeID++;

	FRailNodeData Data;
	Data.ID = NewID;
	Data.WorldPosition = WorldPos;
	Data.Type = Type;
	Data.ActiveEdge = ActiveEdge;

	Nodes.Add(NewID.Value, Data);
	return NewID;
}

FRailEdgeID URailNetworkSubsystem::CreateEdge(FRailNodeID A, FRailNodeID B, const FVector& TangentA, const FVector& TangentB)
{
	FRailEdgeID NewID;
	NewID.Value = NextEdgeID++;

	FRailEdgeData Data;
	Data.ID = NewID;
	Data.NodeA = A;
	Data.NodeB = B;
	Data.TangentA = TangentA;
	Data.TangentB = TangentB;

	RecomputeEdgeDerived(Data);

	Edges.Add(NewID.Value, Data);

	// Register connectivity
	if (FRailNodeData* NA = Nodes.Find(A.Value))	NA->ConnectedEdges.Add(NewID);
	if (FRailNodeData* NB = Nodes.Find(B.Value))	NB->ConnectedEdges.Add(NewID);

	return NewID;
}

bool URailNetworkSubsystem::RemoveEdge(FRailEdgeID Edge)
{
	FRailEdgeData* Data = Edges.Find(Edge.Value);
	if (!Data) return false;

	// Remove from nodes
	if (FRailNodeData* NA = Nodes.Find(Data->NodeA.Value)) NA->ConnectedEdges.Remove(Edge);
	if (FRailNodeData* NB = Nodes.Find(Data->NodeB.Value)) NB->ConnectedEdges.Remove(Edge);

	Edges.Remove(Edge.Value);
	return true;
}

bool URailNetworkSubsystem::GetNodeData(FRailNodeID Node, FRailNodeData& OutData) const
{
	const FRailNodeData* Data = Nodes.Find(Node.Value);
	if (!Data) return false;
	OutData = *Data;
	return true;
}

bool URailNetworkSubsystem::GetEdgeData(FRailEdgeID Edge, FRailEdgeData& OutData) const
{
	const FRailEdgeData* Data = Edges.Find(Edge.Value);
	if (!Data) return false;
	OutData = *Data;
	return true;
}

float URailNetworkSubsystem::GetEdgeLength(FRailEdgeID Edge) const
{
	const FRailEdgeData* Data = Edges.Find(Edge.Value);
	return Data ? Data->Length : 0.f;
}

TArray<FRailEdgeID> URailNetworkSubsystem::GetConnectedEdges(FRailNodeID Node) const
{
	const FRailNodeData* Data = Nodes.Find(Node.Value);
	return Data ? Data->ConnectedEdges : TArray<FRailEdgeID>();
}

// ---------- GEOMETRY ----------

// Returns a full transform given an edge ID, and a distance along said edge.
FTransform URailNetworkSubsystem::GetTransformAtDistance(FRailEdgeID Edge, float S) const
{
	const FRailEdgeData* E = Edges.Find(Edge.Value);
	if (!E) return FTransform::Identity;

	const FRailNodeData* NA = Nodes.Find(E->NodeA.Value);
	const FRailNodeData* NB = Nodes.Find(E->NodeB.Value);
	if (!NA || !NB) return FTransform::Identity;

	const float Len = FMath::Max(E->Length, 1.f);
	const float T = FMath::Clamp(S / Len, 0.f, 1.f);

	// Hermite evaluation (authoritative)
	const FVector Pos = RailMath::EvalHermitePos(
		NA->WorldPosition, E->TangentA,
		NB->WorldPosition, E->TangentB,
		T);

	FVector Tangent = RailMath::EvalHermiteTangent(
		NA->WorldPosition, E->TangentA,
		NB->WorldPosition, E->TangentB,
		T);

	Tangent = Tangent.GetSafeNormal();

	// Build a stable frame (Forward = tangent, Z-up rail frame)

	const FVector Forward = Tangent;
	const FVector WorldUp = FVector::UpVector;

	// If Forward is nearly vertical, choose a different up to avoid degeneracy
	FVector Up = WorldUp;
	if (FMath::Abs(FVector::DotProduct(Forward, WorldUp)) > 0.99f)
	{
		Up = FVector::RightVector;
	}

	// Build orthonormal basis
	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
	const FVector TrueUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();

	// Use UE helper (much more stable than manual FMatrix)
	const FMatrix RotMat = FRotationMatrix::MakeFromXZ(Forward, TrueUp);

	return FTransform(RotMat.Rotator(), Pos);
}

// ---------- MOVEMENT ----------


// The meat and potatoes of our movement system. 
FRailTravelResult URailNetworkSubsystem::AdvanceAlongRails(
	FRailEdgeID Edge,
	float S,
	ERailDirection Dir,
	float DeltaS,
	const FRailMoveContext& Ctx)
{
	FRailTravelResult Result;
	Result.Edge = Edge;
	Result.Dir = Dir;
	Result.S = S;

	float Remaining = DeltaS;

	while (Remaining > 0.f)
	{
		const FRailEdgeData* E = Edges.Find(Result.Edge.Value);
		if (!E)
		{
			Result.bStopped = true;
			Result.StopReason = ERailStopReason::InvalidGraph;
			return Result;
		}

		const float Length = E->Length;

		// Distance available before hitting an endpoint
		float DistToEnd =
			(Result.Dir == ERailDirection::AToB)
			? (Length - Result.S)
			: (Result.S);

		// Case 1: we stay on this edge
		if (Remaining < DistToEnd)
		{
			Result.S += (Result.Dir == ERailDirection::AToB) ? Remaining : -Remaining;
			return Result;
		}

		// Case 2: we reach the node
		Remaining -= DistToEnd;

		// Snap exactly to the endpoint
		Result.S = (Result.Dir == ERailDirection::AToB) ? Length : 0.f;

		// Determine arrival node
		FRailNodeID ArriveNode =
			(Result.Dir == ERailDirection::AToB)
			? E->NodeB
			: E->NodeA;

		Result.LastTransitionNode = ArriveNode;

		// Choose next edge
		FRailEdgeID NextEdge = SelectNextEdge(
			ArriveNode,
			Result.Edge,
			Result.Dir,
			Ctx);

		// If no valid continuation → stop here
		if (!NextEdge.IsValid())
		{
			Result.bStopped = true;
			Result.StopReason = ERailStopReason::NoNextEdge;
			return Result;
		}

		// Optional: signaling / block check
		if (Ctx.bEnforceSignals && !CanEnterEdge(ArriveNode, NextEdge, /*TrainID*/ 0))
		{
			Result.bStopped = true;
			Result.StopReason = ERailStopReason::BlockedBySignal;
			return Result;
		}

		// Switch to the new edge
		const FRailEdgeData* NE = Edges.Find(NextEdge.Value);
		if (!NE)
		{
			Result.bStopped = true;
			Result.StopReason = ERailStopReason::InvalidGraph;
			return Result;
		}

		// Determine new direction on that edge
		if (NE->NodeA.Value == ArriveNode.Value)
		{
			Result.Dir = ERailDirection::AToB;
			Result.S = 0.f;
		}
		else if (NE->NodeB.Value == ArriveNode.Value)
		{
			Result.Dir = ERailDirection::BToA;
			Result.S = NE->Length;
		}
		else
		{
			// Graph inconsistency
			Result.bStopped = true;
			Result.StopReason = ERailStopReason::InvalidGraph;
			return Result;
		}

		Result.Edge = NextEdge;

		// Loop continues with remaining distance
	}

	return Result;
}

FRailEdgeID URailNetworkSubsystem::SelectNextEdge(
	FRailNodeID AtNode,
	FRailEdgeID IncomingEdge,
	ERailDirection IncomingDir,
	const FRailMoveContext& Ctx) const
{
	

	const FRailNodeData* Node = Nodes.Find(AtNode.Value);
	if (!Node) return FRailEdgeID();

	UE_LOG(LogTemp, Warning, TEXT("SelectNextEdge at Node %d, Type=%d, IncomingEdge=%d"),
		AtNode.Value,
		(int32)Node->Type,
		IncomingEdge.Value);

	// Build candidate list = all edges except the one we came from
	TArray<FRailEdgeID> Candidates;
	for (const FRailEdgeID& E : Node->ConnectedEdges)
	{
		if (E.Value != IncomingEdge.Value)
		{
			Candidates.Add(E);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("ConnectedEdges at node %d:"), AtNode.Value);
	for (const FRailEdgeID& E : Node->ConnectedEdges)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Edge %d"), E.Value);
	}

	UE_LOG(LogTemp, Warning, TEXT("Candidates after filtering incoming:"));
	for (const FRailEdgeID& E : Candidates)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Candidate %d"), E.Value);
	}


	if (Candidates.Num() == 0)
	{
		// Dead end
		return FRailEdgeID();
	}

	// -------------------------------------------------
	// 1) Planned path takes highest priority
	// -------------------------------------------------
	if (Ctx.bUsePlannedPath && Ctx.PlannedEdges.IsValidIndex(Ctx.PlannedIndex))
	{
		const FRailEdgeID PlannedNext = Ctx.PlannedEdges[Ctx.PlannedIndex];

		for (const FRailEdgeID& Candidate : Candidates)
		{
			if (Candidate.Value == PlannedNext.Value)
			{
				// NOTE: caller should advance PlannedIndex after successful transition
				return Candidate;
			}
		}

		// Planned path mismatch
		return FRailEdgeID(); // PathMismatch later if you want
	}

	// -------------------------------------------------
	// 2) Switch logic
	// -------------------------------------------------
	if (Node->Type == ERailNodeType::Switch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Node %d is switch. ActiveEdge=%d"),
			AtNode.Value,
			Node->ActiveEdge.Value);

		if (Node->ActiveEdge.IsValid())
		{
			for (const FRailEdgeID& Candidate : Candidates)
			{
				if (Candidate.Value == Node->ActiveEdge.Value)
				{
					return Candidate;
				}
			}

			// Switch set to an edge that doesn't match this approach
			return FRailEdgeID(); // SwitchMismatch
		}
	}

	// -------------------------------------------------
	// 3) Fallback: only one valid way forward
	// -------------------------------------------------
	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}

	// -------------------------------------------------
	// 4) Ambiguous junction → stop
	// -------------------------------------------------
	return FRailEdgeID();
}



// ---------- BLOCKS ----------

FRailBlockID URailNetworkSubsystem::CreateBlock(FName Label, ERailBlockType Type, const TArray<FRailEdgeID>& InEdges)
{
	FRailBlockID NewID;
	NewID.Value = NextBlockID++;

	FRailBlockData Data;
	Data.ID = NewID;
	Data.UserLabel = Label;
	Data.Type = Type;
	Data.MemberEdges = InEdges;

	Blocks.Add(NewID.Value, Data);

	// Build fast lookup
	for (const FRailEdgeID& E : InEdges)
	{
		EdgeToBlock.Add(E.Value, NewID.Value);
	}

	return NewID;
}

FRailBlockID URailNetworkSubsystem::GetBlockForEdge(FRailEdgeID Edge) const
{
	const int32* BlockVal = EdgeToBlock.Find(Edge.Value);
	FRailBlockID ID;
	ID.Value = BlockVal ? *BlockVal : INDEX_NONE;
	return ID;
}

bool URailNetworkSubsystem::IsBlockOccupied(FRailBlockID Block) const
{
	const FRailBlockData* Data = Blocks.Find(Block.Value);
	return Data ? Data->bOccupied : false;
}

// ---------- SIGNALS ----------

FRailSignalID URailNetworkSubsystem::CreateSignal(FRailNodeID Node, FRailEdgeID Edge, FRailBlockID Block)
{
	FRailSignalID NewID;
	NewID.Value = NextSignalID++;

	FRailSignalData Data;
	Data.ID = NewID;
	Data.PlacementNode = Node;
	Data.ControlledEdge = Edge;
	Data.TargetBlock = Block;

	Signals.Add(NewID.Value, Data);
	return NewID;
}

bool URailNetworkSubsystem::CanEnterEdge(FRailNodeID AtNode, FRailEdgeID NextEdge, int32 TrainID) const
{
	// MVP: always allow
	return true;
}

// ---------- INTERNAL HELPERS ----------

void URailNetworkSubsystem::RecomputeEdgeDerived(FRailEdgeData& EdgeData)
{
	const FRailNodeData* NA = Nodes.Find(EdgeData.NodeA.Value);
	const FRailNodeData* NB = Nodes.Find(EdgeData.NodeB.Value);
	if (!NA || !NB)
	{
		EdgeData.Length = 0.f;
		return;
	}

	// Very rough length approximation (straight-line for now)
	EdgeData.Length = FVector::Distance(NA->WorldPosition, NB->WorldPosition);

	// Placeholder derived values
	EdgeData.SpeedLimit = 1000.f;
}
