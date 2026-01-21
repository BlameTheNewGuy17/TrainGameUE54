// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/RailNetworkSubsystem.h"
#include "RailMath.h"

/*
URailNetworkSubsystem
Implementation skeleton for the rail simulation core.
This file intentionally contains minimal logic so the project links and you can
incrementally fill in behavior starting with GetTransformAtDistance.
*/

// ---------- CORE GRAPH ----------

FRailNodeID URailNetworkSubsystem::CreateNode(const FVector& WorldPos, ERailNodeType Type)
{
	FRailNodeID NewID;
	NewID.Value = NextNodeID++;

	FRailNodeData Data;
	Data.ID = NewID;
	Data.WorldPosition = WorldPos;
	Data.Type = Type;

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

	// Build a simple rotation frame (Up = +Z for now)
	const FVector Up = FVector::UpVector;
	const FVector Right = FVector::CrossProduct(Up, Tangent).GetSafeNormal();
	const FVector TrueUp = FVector::CrossProduct(Tangent, Right).GetSafeNormal();

	const FMatrix RotMat(
		FPlane(Tangent, 0),
		FPlane(Right, 0),
		FPlane(TrueUp, 0),
		FPlane(0, 0, 0, 1));

	return FTransform(RotMat.Rotator(), Pos);
}

// ---------- MOVEMENT ----------

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
	Result.S = S + ((Dir == ERailDirection::AtoB) ? DeltaS : -DeltaS);

	// MVP version: no transitions yet
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

	// Simple rule: if exactly two edges, take the other one
	if (Node->ConnectedEdges.Num() == 2)
	{
		return (Node->ConnectedEdges[0].Value == IncomingEdge.Value)
			? Node->ConnectedEdges[1]
			: Node->ConnectedEdges[0];
	}

	// Otherwise: no valid continuation (stop)
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
