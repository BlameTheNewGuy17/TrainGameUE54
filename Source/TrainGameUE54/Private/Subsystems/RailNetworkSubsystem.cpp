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

void URailNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Error, TEXT("RailNetworkSubsystem: World type: %d"), (int32)GetWorld()->WorldType);

}

void URailNetworkSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
}

void URailNetworkSubsystem::ClearDebugDraw()
{
	FlushPersistentDebugLines(GetWorld());
}

void URailNetworkSubsystem::DebugDrawRailNetwork(float Duration, float Thickness) const
{
	UWorld* World = GetWorld();

	// ---- Draw Nodes ----
	for (const auto& Pair : Nodes)
	{
		const FRailNodeData & Node = Pair.Value;

		DrawDebugSphere(
			World,
			Node.Transform.GetLocation(),
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

		FVector PrevPos = NA->Transform.GetLocation();

		for (int32 i = 1; i <= NumSegments; ++i)
		{
			const float T = (float)i / (float)NumSegments;

			const FVector Pos = RailMath::EvalHermitePos(
				NA->Transform.GetLocation(), Edge.TangentA,
				NB->Transform.GetLocation(), Edge.TangentB,
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
			NA->Transform.GetLocation(),
			NA->Transform.GetLocation() + Edge.TangentA,
			FColor::Green,
			false,
			Duration,
			0,
			2.f);

		DrawDebugLine(
			World,
			NB->Transform.GetLocation(),
			NB->Transform.GetLocation() + Edge.TangentB,
			FColor::Red,
			false,
			Duration,
			0,
			2.f);
	}
}

void URailNetworkSubsystem::DrawWithPDI(FPrimitiveDrawInterface* PDI) const
{
	for (const auto& Pair : Nodes)
	{
		const FRailNodeData& Node = Pair.Value;
		PDI->DrawPoint(Node.Transform.GetLocation(), FLinearColor::Yellow, 10.f, SDPG_Foreground);
	}

	for (const auto& Pair : Edges)
	{
		const FRailEdgeData& Edge = Pair.Value;
		const FRailNodeData* NA = Nodes.Find(Edge.NodeA.Value);
		const FRailNodeData* NB = Nodes.Find(Edge.NodeB.Value);
		if (!NA || !NB) continue;

		FVector Prev = NA->Transform.GetLocation();
		for (int32 i = 1; i <= 32; ++i)
		{
			const float T = (float)i / 32.f;
			FVector Curr = RailMath::EvalHermitePos(NA->Transform.GetLocation(), Edge.TangentA, NB->Transform.GetLocation(), Edge.TangentB, T);
			PDI->DrawLine(Prev, Curr, FLinearColor::Blue, SDPG_Foreground, 2.f);
			Prev = Curr;
		}
	}
}

FRailNodeID URailNetworkSubsystem::CreateNode(const FTransform& WorldTransform, ERailNodeType Type)
{
	ensureMsgf(Type == ERailNodeType::Control,
		TEXT("CreateNode used for non-control node. Please use alternative functions."));

	FRailNodeID NewID;
	NewID.Value = NextNodeID++;

	FRailNodeData Node;
	Node.ID = NewID;
	Node.Transform = WorldTransform;
	Node.Type = Type;

	Nodes.Add(NewID.Value, Node);
	return NewID;
}

FRailNodeID URailNetworkSubsystem::CreateSwitchNode(const FTransform& WorldTransform, const FSwitchNodeData& Data)
{
	FRailNodeID NewID = CreateNode(WorldTransform, ERailNodeType::Switch);
	Switches.Add(NewID.Value, Data);
	return NewID;
}

FRailNodeID URailNetworkSubsystem::CreateCrossoverNode(const FTransform& WorldTransform, const FCrossoverNodeData& Data)
{
	FRailNodeID NewID = CreateNode(WorldTransform, ERailNodeType::Crossover);
	Crossovers.Add(NewID.Value, Data);
	return NewID;
}

FRailEdgeID URailNetworkSubsystem::CreateEdge(FRailNodeID A, FRailNodeID B, const FVector* TangentA, const FVector* TangentB)
{
	FRailEdgeID NewID;
	NewID.Value = NextEdgeID++;

	FRailEdgeData Data;
	Data.ID = NewID;
	Data.NodeA = A;
	Data.NodeB = B;

	if (TangentA && TangentB)
	{
		Data.TangentA = *TangentA;
		Data.TangentB = *TangentB;
		RecomputeEdgeLength(Data);
	}
	else
	{
		RecomputeEdgeDerived(Data);
	}

	Edges.Add(NewID.Value, Data);

	if (FRailNodeData* NA = Nodes.Find(A.Value)) NA->ConnectedEdges.Add(NewID);
	if (FRailNodeData* NB = Nodes.Find(B.Value)) NB->ConnectedEdges.Add(NewID);

	UpdateNodeType(A);
	UpdateNodeType(B);

	return NewID;
}

void URailNetworkSubsystem::SetSwitchActiveEdge(FRailNodeID NodeID, FRailEdgeID EdgeID)
{
	if (FSwitchNodeData* Switch = Switches.Find(NodeID.Value))
	{
		Switch->ActiveEdge = EdgeID;
	}
}

void URailNetworkSubsystem::SetNodeTransform(FRailNodeID NodeID, const FTransform& NewTransform)
{
	if (FRailNodeData* Node = Nodes.Find(NodeID.Value))
		Node->Transform = NewTransform;
}

void URailNetworkSubsystem::SetNodeType(FRailNodeID NodeID, ERailNodeType NewType)
{
	if (FRailNodeData* Node = Nodes.Find(NodeID.Value))
		Node->Type = NewType;
}

bool URailNetworkSubsystem::RemoveEdge(FRailEdgeID Edge)
{
	FRailEdgeData* Data = Edges.Find(Edge.Value);
	if (!Data) return false;

	// Remove from nodes
	if (FRailNodeData* NA = Nodes.Find(Data->NodeA.Value)) NA->ConnectedEdges.Remove(Edge);
	if (FRailNodeData* NB = Nodes.Find(Data->NodeB.Value)) NB->ConnectedEdges.Remove(Edge);

	Edges.Remove(Edge.Value);

	UpdateNodeType(Data->NodeA);
	UpdateNodeType(Data->NodeB);
	return true;
}

bool URailNetworkSubsystem::GetNodeData(FRailNodeID Node, FRailNodeData& OutData) const
{
	const FRailNodeData* Data = Nodes.Find(Node.Value);
	if (!Data) return false;
	OutData = *Data;
	return true;
}

bool URailNetworkSubsystem::GetSwitchData(FRailNodeID Node, FSwitchNodeData& OutData) const
{
	const FSwitchNodeData* Data = Switches.Find(Node.Value);
	if (!Data) return false;
	OutData = *Data;
	return true;
}

bool URailNetworkSubsystem::GetCrossoverData(FRailNodeID Node, FCrossoverNodeData& OutData) const
{
	const FCrossoverNodeData* Data = Crossovers.Find(Node.Value);
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

void URailNetworkSubsystem::OnNodeTransformChanged(FRailNodeID NodeID)
{
	FRailNodeData* Node = Nodes.Find(NodeID.Value);
	if (!Node) return;

	for (FRailEdgeID EdgeID : Node->ConnectedEdges)
	{
		FRailEdgeData* Edge = Edges.Find(EdgeID.Value);
		if (!Edge) continue;

		bool bIsNodeA = Edge->NodeA.Value == NodeID.Value;
		FRailNodeID OtherNodeID = bIsNodeA ? Edge->NodeB : Edge->NodeA;
		FRailNodeData* OtherNode = Nodes.Find(OtherNodeID.Value);
		if (!OtherNode) continue;

		FVector HintDir = bIsNodeA ? Edge->TangentA.GetSafeNormal() : Edge->TangentB.GetSafeNormal();
		FVector NewTangent = GetContinuationTangent(NodeID, HintDir);
		float Dist = FVector::Distance(Node->Transform.GetLocation(), OtherNode->Transform.GetLocation());

		if (bIsNodeA)
			Edge->TangentA = NewTangent * Dist;
		else
			Edge->TangentB = NewTangent * Dist;

		RecomputeEdgeLength(*Edge);
	}
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
		NA->Transform.GetLocation(), E->TangentA,
		NB->Transform.GetLocation(), E->TangentB,
		T);

	FVector Tangent = RailMath::EvalHermiteTangent(
		NA->Transform.GetLocation(), E->TangentA,
		NB->Transform.GetLocation(), E->TangentB,
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
	UE_LOG(LogTemp, Warning, TEXT("Edge %d Length=%.1f S=%.1f T=%.4f Pos=%s"),
		Edge.Value, E->Length, S, T, *Pos.ToString());
	return FTransform(RotMat.Rotator(), Pos);
}

FVector URailNetworkSubsystem::GetTangentForEdgeAtNode(FRailNodeID NodeID, FRailEdgeID EdgeID) const
{
	const FRailNodeData* Node = Nodes.Find(NodeID.Value);
	if (!Node) return FVector::ForwardVector;

	const FVector Forward = Node->Transform.GetUnitAxis(EAxis::X);
	const FVector Up = Node->Transform.GetUnitAxis(EAxis::Z);

	// Control and Switch nodes - all edges share BaseTangent
	if (Node->Type != ERailNodeType::Crossover)
	{
		return Forward;
	}

	// Crossover - look up pair angle for this edge
	const FCrossoverNodeData* Crossover = Crossovers.Find(NodeID.Value);
	if (!Crossover) return Forward;

	const float* Angle = Crossover->PairAngles.Find(EdgeID);
	if (!Angle) return Forward;

	FQuat Rot = FQuat(Up, FMath::DegreesToRadians(*Angle));
	return Rot.RotateVector(Forward).GetSafeNormal();
}

FVector URailNetworkSubsystem::GetContinuationTangent(FRailNodeID NodeID, const FVector& HintDirection) const
{
	const FRailNodeData* Node = Nodes.Find(NodeID.Value);
	if (!Node) return HintDirection;

	FVector Forward = Node->Transform.GetUnitAxis(EAxis::X);

	// If hint is within 90° of forward, use forward, otherwise flip
	float Dot = FVector::DotProduct(Forward, HintDirection.GetSafeNormal());
	return Dot >= 0.f ? Forward : -Forward;
}

bool URailNetworkSubsystem::FindClosestRailLocation(FVector WorldPos, FRailLocation& Out, float& OutDistSq) const
{
	const float SampleStep = 100.f;      // cm — coarse pass resolution
	const float RefineStep = 20.f;       // cm — refinement resolution
	const int32 RefineIterations = 5;

	bool bFound = false;
	float BestDistSq = TNumericLimits<float>::Max();
	FRailLocation BestLoc;

	for (const auto& Pair : Edges)
	{
		const FRailEdgeID EdgeID(Pair.Key);
		const FRailEdgeData& Edge = Pair.Value;

		if (Edge.Length <= KINDA_SMALL_NUMBER)
			continue;

		float BestSOnEdge = 0.f;
		float LocalBestDist = TNumericLimits<float>::Max();

		// ---- Coarse sampling pass ----
		for (float S = 0.f; S <= Edge.Length; S += SampleStep)
		{
			const FVector Pos = GetTransformAtDistance(EdgeID, S).GetLocation();
			const float DistSq = FVector::DistSquared(Pos, WorldPos);

			if (DistSq < LocalBestDist)
			{
				LocalBestDist = DistSq;
				BestSOnEdge = S;
			}
		}

		// ---- Refinement pass ----
		float Step = RefineStep;
		float Center = BestSOnEdge;

		for (int32 i = 0; i < RefineIterations; ++i)
		{
			float Start = FMath::Max(0.f, Center - Step);
			float End = FMath::Min(Edge.Length, Center + Step);

			for (float S = Start; S <= End; S += Step)
			{
				const FVector Pos = GetTransformAtDistance(EdgeID, S).GetLocation();
				const float DistSq = FVector::DistSquared(Pos, WorldPos);

				if (DistSq < LocalBestDist)
				{
					LocalBestDist = DistSq;
					Center = S;
				}
			}

			Step *= 0.5f;
		}

		// ---- Compare against global best ----
		if (LocalBestDist < BestDistSq)
		{
			BestDistSq = LocalBestDist;
			BestLoc.Edge = EdgeID;
			BestLoc.S = Center;
			bFound = true;
		}
	}

	if (bFound)
	{
		Out = BestLoc;
		OutDistSq = BestDistSq;
	}

	return bFound;
}

FRailNodeID URailNetworkSubsystem::FindNearestNode(const FVector& WorldPos, float MaxDistanceCm) const
{
	FRailNodeID BestID;
	float BestDistSq = FMath::Square(MaxDistanceCm);

	for (const auto& Pair : Nodes)
	{
		const float DistSq = FVector::DistSquared(Pair.Value.Transform.GetLocation(), WorldPos);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestID = Pair.Value.ID;
		}
	}

	return BestID; // Invalid if nothing found within threshold
}

// ---------- CONSTRAINT SOLVER ----------

bool URailNetworkSubsystem::GetPositionAndTangent(
	const FRailLocation& Loc,FVector& OutPos,FVector& OutdPdS) const
{
	return true;
}

bool URailNetworkSubsystem::SolveTrailingForLinearDistance(	const FRailLocation& Anchor,const FVector& AnchorPos,float TargetDist,const FRailLocation& InitialGuess,const FRailMoveContext& Ctx,FRailLocation& OutSolved,int32 MaxNewtonIters,float ToleranceCm)
{

	const float Tol2 = FMath::Square(ToleranceCm);
	const float TargetDist2 = FMath::Square(TargetDist);

	// Start from last frame's answer (temporal coherence)
	FRailLocation X = InitialGuess;

	// Binary search bounds: 0 offset = same point, max offset = TargetDist (straight track)
	float Lo = 0.f;
	float Hi = TargetDist * 1.5f; // overshoot a bit for very curved track

	for (int32 i = 0; i < MaxNewtonIters; ++i)
	{
		const float Mid = (Lo + Hi) * 0.5f;

		// Sample point at this rail offset behind anchor
		FRailTravelResult R = AdvanceAlongRails(Anchor, -Mid, Ctx);
		FVector SamplePos = GetTransformAtDistance(R.Edge, R.S).GetLocation();

		const float Dist2 = FVector::DistSquared(SamplePos, AnchorPos);

		if (Dist2 < TargetDist2)
			Lo = Mid; // too close, go further back
		else
			Hi = Mid; // too far, come forward

		// Converged?
		if (FMath::Abs(Dist2 - TargetDist2) < Tol2 * TargetDist * 2.f)
			break;
	}
	
	FRailTravelResult Final = AdvanceAlongRails(Anchor, -(Lo + Hi) * 0.5f, Ctx);
	OutSolved.Edge = Final.Edge;
	OutSolved.Dir = Final.Dir;
	OutSolved.S = Final.S;
	return true;
}


// ---------- MOVEMENT ----------

// The meat and potatoes of our movement system. 

// Move point along the network. 
FRailTravelResult URailNetworkSubsystem::AdvanceAlongRails(	FRailLocation Location,	float DeltaS,	const FRailMoveContext& Ctx) const
{

	FRailTravelResult Result;
	Result.Edge = Location.Edge;
	Result.Dir = Location.Dir;
	Result.S = Location.S;

	float Remaining = FMath::Abs(DeltaS);
	if (DeltaS < 0.f)
	{
		Result.Dir = Opposite(Location.Dir);
	}


	while (Remaining > KINDA_SMALL_NUMBER)
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
		if (Remaining <= KINDA_SMALL_NUMBER)
		{
			// We landed exactly on the node; stop cleanly
			return Result;
		}
		

		// Snap exactly to the endpoint
		Result.S = (Result.Dir == ERailDirection::AToB) ? Length : 0.f;

		// Determine arrival node
		FRailNodeID ArriveNode =
			(Result.Dir == ERailDirection::AToB)
			? E->NodeB
			: E->NodeA;

		Result.LastTransitionNode = ArriveNode;

		UE_LOG(LogTemp, Warning,
			TEXT("[Advance] Hit node %d on Edge %d | DistToEnd=%.3f Remaining=%.3f Dir=%d"),
			ArriveNode.Value,
			Result.Edge.Value,
			DistToEnd,
			Remaining,
			(int32)Result.Dir);

		// Choose next edge
		FRailEdgeID NextEdge = SelectNextEdge(
			ArriveNode,
			Result.Edge,
			Result.Dir,
			Ctx);
	

		// If no valid continuation -> stop here
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
		

		// IMPORTANT: commit edge change immediately
		Result.Edge = NextEdge;

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

		// Graph inconsistency
		else
		{
			Result.bStopped = true;
			Result.StopReason = ERailStopReason::InvalidGraph;
			return Result;
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[Advance] Enter Edge %d at s=%.3f Dir=%d (Len=%.3f)"),
			Result.Edge.Value,
			Result.S,
			(int32)Result.Dir,
			NE->Length);
	}

	return Result;
}

FRailEdgeID URailNetworkSubsystem::SelectNextEdge(	FRailNodeID AtNode,	FRailEdgeID IncomingEdge,	ERailDirection IncomingDir,	const FRailMoveContext& Ctx) const
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
		const bool bIsIncoming = (E.Value == IncomingEdge.Value);
		if (!bIsIncoming || Ctx.bAllowUTurn)
		{
			Candidates.Add(E);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("ConnectedEdges at Node %d:"), AtNode.Value);
	for (const FRailEdgeID& E : Node->ConnectedEdges)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Edge %d"), E.Value);
	}

	UE_LOG(LogTemp, Warning, TEXT("Candidates after filtering incoming:"));
	for (const FRailEdgeID& E : Candidates)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Candidate %d"), E.Value);
	}

	// -------------------------------------------------
	// 0) Dead end -> stop
	// -------------------------------------------------
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
		if (const FSwitchNodeData* Switch = Switches.Find(AtNode.Value))
		{
			UE_LOG(LogTemp, Warning, TEXT("Node %d is switch. ActiveEdge=%d"),
				AtNode.Value,
				Switch->ActiveEdge.Value);

			if (Switch->ActiveEdge.IsValid())
			{
				for (const FRailEdgeID& Candidate : Candidates)
				{
					if (Candidate.Value == Switch->ActiveEdge.Value)
					{
						return Candidate;
					}
				}

				return FRailEdgeID(); // SwitchMismatch
			}
		}
		else
		
		UE_LOG(LogTemp, Warning, TEXT("Node %d marked Switch but has no SwitchNodeData"), AtNode.Value);
		{
		FRailEdgeID(); // SwitchMismatch
		}
	}

	// -------------------------------------------------
	// 3) Fallback: only one valid way forward
	// -------------------------------------------------
	if (Node->Type == ERailNodeType::Crossover)
	{
		if (const FCrossoverNodeData* Crossover = Crossovers.Find(AtNode.Value))
		{
			if (const FRailEdgeID* Exit = Crossover->PairMap.Find(IncomingEdge))
			{
				return *Exit;
			}
			
			return FRailEdgeID(); // CrossoverMismatch
		}
	}
	
	// -------------------------------------------------
	// 4) Fallback: only one valid way forward
	// -------------------------------------------------
	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}

	// -------------------------------------------------
	// 5) Ambiguous junction -> stop
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

	FVector PosA = NA->Transform.GetLocation();
	FVector PosB = NB->Transform.GetLocation();
	float StraightDist = FVector::Distance(PosA, PosB);

	EdgeData.TangentA = GetTangentForEdgeAtNode(EdgeData.NodeA, EdgeData.ID) * StraightDist;
	EdgeData.TangentB = GetTangentForEdgeAtNode(EdgeData.NodeB, EdgeData.ID) * StraightDist;

	RecomputeEdgeLength(EdgeData);
}

void URailNetworkSubsystem::RecomputeEdgeLength(FRailEdgeData& EdgeData)
{
	const FRailNodeData* NA = Nodes.Find(EdgeData.NodeA.Value);
	const FRailNodeData* NB = Nodes.Find(EdgeData.NodeB.Value);
	if (!NA || !NB)
	{
		EdgeData.Length = 0.f;
		return;
	}

	const int32 NumSteps = 32;
	float Length = 0.f;
	FVector PosA = NA->Transform.GetLocation();
	FVector PosB = NB->Transform.GetLocation();
	FVector Prev = RailMath::EvalHermitePos(PosA, EdgeData.TangentA, PosB, EdgeData.TangentB, 0.f);

	for (int32 i = 1; i <= NumSteps; ++i)
	{
		const float T = (float)i / (float)NumSteps;
		FVector Curr = RailMath::EvalHermitePos(PosA, EdgeData.TangentA, PosB, EdgeData.TangentB, T);
		Length += FVector::Distance(Prev, Curr);
		Prev = Curr;
	}

	EdgeData.Length = Length;
	EdgeData.SpeedLimit = 1000.f;
}

void URailNetworkSubsystem::UpdateNodeType(FRailNodeID NodeID)
{
	FRailNodeData* Node = Nodes.Find(NodeID.Value);
	if (!Node) return;

	const int32 EdgeCount = Node->ConnectedEdges.Num();

	if (EdgeCount <= 2)
		Node->Type = ERailNodeType::Control;
	else if (EdgeCount == 3)
		Node->Type = ERailNodeType::Switch;
	else
		Node->Type = ERailNodeType::Crossover;
}
