// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystems/RailNetworkSubsystem.h"
#include "RailMath.h"
#include "RailNetworkSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// ---------- INIT ----------

void URailNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("RailNetworkSubsystem: Initialized"));
}

void URailNetworkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
}

// ---------- INTERNAL GRAPH OPS ----------

FRailNodeID URailNetworkSubsystem::CreateNode(const FTransform& WorldTransform)
{
    FRailNodeID NewID;
    NewID.Value = Graph.AddNode(WorldTransform);

    FRailNodeData Node;
    Node.ID = NewID;
    Node.Type = ERailNodeType::Control;
    Node.WorldTransform = WorldTransform;

    RailNodes.Add(NewID.Value, Node);
    return NewID;
}

FRailEdgeID URailNetworkSubsystem::CreateEdge(FRailNodeID A, FRailNodeID B, const FVector& TangentA, const FVector& TangentB)
{
    FRailEdgeID NewID;
    NewID.Value = Graph.AddEdge(A.Value, B.Value);

    FRailEdgeData Data;
    Data.ID = NewID;
    Data.NodeA = A;
    Data.NodeB = B;
    Data.TangentA = TangentA;
    Data.TangentB = TangentB;
    RecomputeEdgeLength(Data);

    RailEdges.Add(NewID.Value, Data);

    // Mirror connectivity and store edge angles
    if (FRailNodeData* NA = RailNodes.Find(A.Value))
    {
        NA->ConnectedEdges.Add(NewID);
        NA->EdgeAngles.Add(NewID, ComputeEdgeAngleAtNode(A, TangentA));
    }
    if (FRailNodeData* NB = RailNodes.Find(B.Value))
    {
        NB->ConnectedEdges.Add(NewID);
        NB->EdgeAngles.Add(NewID, ComputeEdgeAngleAtNode(B, TangentB));
    }

    UpdateNodeType(A);
    UpdateNodeType(B);

    return NewID;
}

bool URailNetworkSubsystem::RemoveEdge(FRailEdgeID EdgeID)
{
    FRailEdgeData* Data = RailEdges.Find(EdgeID.Value);
    if (!Data) return false;

    FRailNodeID NodeA = Data->NodeA;
    FRailNodeID NodeB = Data->NodeB;

    // Mirror connectivity removal
    if (FRailNodeData* NA = RailNodes.Find(NodeA.Value)) NA->ConnectedEdges.Remove(EdgeID), NA->EdgeAngles.Remove(EdgeID);
    if (FRailNodeData* NB = RailNodes.Find(NodeB.Value)) NB->ConnectedEdges.Remove(EdgeID), NB->EdgeAngles.Remove(EdgeID);

    RailEdges.Remove(EdgeID.Value);
    Graph.RemoveEdge(EdgeID.Value);

    // UpdateNodeType handles auto-remove if EdgeCount hits 0
    UpdateNodeType(NodeA);
    UpdateNodeType(NodeB);

    return true;
}

bool URailNetworkSubsystem::RemoveNode(FRailNodeID NodeID)
{
    RailNodes.Remove(NodeID.Value);
    Switches.Remove(NodeID.Value);
    Graph.RemoveNode(NodeID.Value);
    return true;
}

// ---------- INTERNAL VALIDATION ----------

bool URailNetworkSubsystem::ValidateEdgeRequest(const FEdgePlacementRequest& Request) const
{
    

    FVector TanA, TanB;
    ResolveRequestTangents(Request, TanA, TanB);

    FVector PosA, PosB;
    if (Request.ExistingNodeA.IsValid())
    {
        const FNodeData* NA = Graph.GetNodeData(Request.ExistingNodeA.Value);
        if (!NA) return false;
        PosA = NA->Transform.GetLocation();
        UE_LOG(LogTemp, Warning, TEXT("ValidateEdge: Checking NodeA=%d with IncomingTan=%s"),
            Request.ExistingNodeA.Value, *(TanA).ToString());
        if (!CanAddEdgeToNode(Request.ExistingNodeA, TanA))
            return false;
    }


    else
    {
        PosA = Request.TransformA.GetLocation();
    }

    if (Request.ExistingNodeB.IsValid())
    {
        const FNodeData* NB = Graph.GetNodeData(Request.ExistingNodeB.Value);
        if (!NB) return false;
        PosB = NB->Transform.GetLocation();
        UE_LOG(LogTemp, Warning, TEXT("ValidateEdge: Checking NodeB=%d with IncomingTan=%s"),
            Request.ExistingNodeB.Value, *(TanB).ToString());
        if (!CanAddEdgeToNode(Request.ExistingNodeB, TanB))
            return false;
    }

    else
    {
        PosB = Request.TransformB.GetLocation();
    }

    if (!CheckCurveRadius(PosA, TanA, PosB, TanB))
        return false;

    return true;
}

void URailNetworkSubsystem::ResolveRequestTangents(const FEdgePlacementRequest& Request, FVector& OutTanA, FVector& OutTanB) const
{
    OutTanA = Request.TangentA.GetSafeNormal();
    OutTanB = Request.TangentB.GetSafeNormal();

    auto ResolveNode = [&](FRailNodeID NodeID, FVector& OutTan, bool bIsNewNodeA)
        {
            // Get the RailNodeData for the provided NodeID
            const FRailNodeData* Node = RailNodes.Find(NodeID.Value);
            // Bail if there are no stored EdgeAngles. ConnectedEdges should also be empty.
            if (!Node || Node->EdgeAngles.Num() == 0) return;

            // Not sure what we do here. Bail if we aren't a control and we have more than 2 edges? I think the point is that we know our type so bail.
            if (Node->Type != ERailNodeType::Control && Node->ConnectedEdges.Num() >= 2) return;

            // BuildAngle should be the players intent....this returns a vectors angle relative to the nodes forward vector..
            float BuildAngle = ComputeEdgeAngleAtNode(NodeID, OutTan);


            // Find closest family edge
            // Declare EdgeID storage
            FRailEdgeID ClosestEdgeID;
            // fuck if I know. it does a thing
            float ClosestDiff = TNumericLimits<float>::Max();


            // Loop through all edge angles
            for (const auto& Pair : Node->EdgeAngles)
            {
                // Find the difference between the cursor and the stored angle
                float Diff = FMath::Abs(BuildAngle - Pair.Value);
                // clamp to 180 - I think
                Diff = FMath::Min(Diff, 180.f - Diff);
                if (Diff < ClosestDiff)
                {
                    ClosestDiff = Diff;
                    ClosestEdgeID = Pair.Key;
                }
            }

            // Bail if the new ClosestEdgeID is invalid.
            if (!ClosestEdgeID.IsValid()) return;

            // Store the RailEdgeData of that new EdgeID. This is so we can access it's A and B nodes.
            const FRailEdgeData* ExistingEdge = RailEdges.Find(ClosestEdgeID.Value);
            // Bail if it's data is invalid.
            if (!ExistingEdge) return;

            // Not sure what we're doing here. I think it's part of the A-A flip.
            bool bExistingIsNodeA = ExistingEdge->NodeA.Value == NodeID.Value;
            FVector ExistingTan = bExistingIsNodeA
                ? ExistingEdge->TangentA.GetSafeNormal()
                : ExistingEdge->TangentB.GetSafeNormal();

            // If we have a dead end, we need to figure out what to do with it. 
            if (Node->ConnectedEdges.Num() == 1)
            {
                // Just use the cursor tangent directly, scaled to match the family axis
                // The family axis is the node forward rotated by FamilyAngle
                const FNodeData* GraphNode = Graph.GetNodeData(NodeID.Value);
                FVector NodeForward = GraphNode->Transform.GetUnitAxis(EAxis::X);
                FVector NodeUp = GraphNode->Transform.GetUnitAxis(EAxis::Z);
                float FamilyAngle = Node->EdgeAngles[ClosestEdgeID];
                FVector FamilyAxis = FQuat(NodeUp, FMath::DegreesToRadians(FamilyAngle))
                    .RotateVector(NodeForward);

                // Snap cursor to family axis, preserving cursor direction
                float CursorDot = FVector::DotProduct(OutTan, FamilyAxis);
                OutTan = CursorDot >= 0.f ? FamilyAxis : -FamilyAxis;
            }

            UE_LOG(LogTemp, Warning, TEXT("ResolveNode %d: BuildAngle=%.1f ClosestAngle=%.1f Diff=%.1f OutTan=%s"),
                NodeID.Value, BuildAngle, Node->EdgeAngles[ClosestEdgeID], ClosestDiff, *OutTan.ToString());
        };

    if (Request.ExistingNodeA.IsValid()) ResolveNode(Request.ExistingNodeA, OutTanA, true);
    if (Request.ExistingNodeB.IsValid()) ResolveNode(Request.ExistingNodeB, OutTanB, false);
}

bool URailNetworkSubsystem::CanAddEdgeToNode(FRailNodeID NodeID, const FVector& IncomingTangent) const
{
    const FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node) return false;

    const int32 EdgeCount = Node->ConnectedEdges.Num();

    if (EdgeCount == 0) return true;
    if (EdgeCount == 1) return true;

    // Compute incoming angle relative to node forward
    float IncomingAngle = ComputeEdgeAngleAtNode(NodeID, IncomingTangent);

    // Find closest existing family
    float ClosestFamilyAngle = TNumericLimits<float>::Max();
    float ClosestDiff = TNumericLimits<float>::Max();
    int32 ClosestFamilyCount = 0;

    // Cluster existing edges into families
    struct FFamily { float RepAngle; int32 Count; };
    TArray<FFamily> Families;

    for (const auto& Pair : Node->EdgeAngles)
    {
        bool bFound = false;
        for (FFamily& Family : Families)
        {
            float Diff = FMath::Abs(Pair.Value - Family.RepAngle);
            Diff = FMath::Min(Diff, 180.f - Diff);
            if (Diff <= 15.f)
            {
                Family.Count++;
                bFound = true;
                break;
            }
        }
        if (!bFound)
            Families.Add({ Pair.Value, 1 });
    }

    // Find which family the incoming angle belongs to
    FFamily* MatchingFamily = nullptr;
    for (FFamily& Family : Families)
    {
        float Diff = FMath::Abs(IncomingAngle - Family.RepAngle);
        Diff = FMath::Min(Diff, 180.f - Diff);
        if (Diff <= 15.f)
        {
            MatchingFamily = &Family;
            break;
        }
    }

    bool bIsNewFamily = (MatchingFamily == nullptr);

    // Type lock checks — must happen before capacity checks
    if (Node->Type == ERailNodeType::Switch && bIsNewFamily)
    {
        UE_LOG(LogTemp, Warning, TEXT("CanAddEdgeToNode: Node %d is locked as Switch, rejecting new angle family"), NodeID.Value);
        return false;
    }
    if (Node->Type == ERailNodeType::Crossover && !bIsNewFamily && Families.Num() == 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("CanAddEdgeToNode: Node %d is locked as Crossover, rejecting switch angle"), NodeID.Value);
        return false;
    }

    if (!bIsNewFamily)
    {
        // Adding to existing family
        // Max 3 edges per family for Switch (trunk + 2 branches)
        // Max 2 edges per family for Crossover
        
        // A Control node with 2 edges can accept a 3rd if it would form a valid switch
        // (incoming angle matches an existing family — promotion to Switch happens in UpdateNodeType)
        if (Node->Type == ERailNodeType::Control && EdgeCount == 2)
            return true;

        int32 MaxPerFamily = (Node->Type == ERailNodeType::Switch) ? 3 : 2;
        if (MatchingFamily->Count >= MaxPerFamily) 
            return false;
        return true;
        
    }
    else
    {
        // New angle family — must be a crossover
        // Max 4 families
        if (Families.Num() >= 4) return false;
        // Max 8 edges total
        if (EdgeCount >= 8) return false;
        return true;
    }
}

bool URailNetworkSubsystem::CheckCurveRadius(const FVector& PosA, const FVector& TanA, const FVector& PosB, const FVector& TanB) const
{
    UE_LOG(LogTemp, Warning, TEXT("CheckCurveRadius: A | %s | %s | B | %s | %s"), *PosA.ToString(), *TanA.ToString(), *PosB.ToString(), *TanB.ToString());
    if (MinCurveRadiusCm <= 0.f) return true;

    const float ChordLen = FVector::Distance(PosA, PosB);
    if (ChordLen < KINDA_SMALL_NUMBER) return true;

    const FVector NormTanA = TanA.GetSafeNormal() * ChordLen;
    const FVector NormTanB = TanB.GetSafeNormal() * ChordLen;

    // Sample the curve
    const int32 NumSamples = 16;
    TArray<FVector> Points;
    Points.Reserve(NumSamples + 1);
    for (int32 i = 0; i <= NumSamples; ++i)
    {
        const float T = (float)i / (float)NumSamples;
        Points.Add(RailMath::EvalHermitePos(PosA, NormTanA, PosB, NormTanB, T));
    }

    // Check circumradius of every consecutive triplet
    for (int32 i = 0; i < Points.Num() - 2; ++i)
    {
        const FVector& A = Points[i];
        const FVector& B = Points[i + 1];
        const FVector& C = Points[i + 2];

        const float AB = FVector::Distance(A, B);
        const float BC = FVector::Distance(B, C);
        const float CA = FVector::Distance(C, A);

        const float CrossMag = FVector::CrossProduct(B - A, C - A).Size();
        if (CrossMag < KINDA_SMALL_NUMBER) continue; // Straight, no curvature

        const float Radius = (AB * BC * CA) / (2.f * CrossMag);

        if (Radius < MinCurveRadiusCm)
        {
            UE_LOG(LogTemp, Warning, TEXT("CheckCurveRadius: Radius %.1f below minimum %.1f"), Radius, MinCurveRadiusCm);
            return false;
        }
    }

    return true;
}

// ---------- PLACEMENT API ----------

bool URailNetworkSubsystem::CanPlaceEdge(const FEdgePlacementRequest& Request) const
{
    return ValidateEdgeRequest(Request);
}

bool URailNetworkSubsystem::RequestPlaceEdge(const FEdgePlacementRequest& Request)
{
    if (!CanPlaceEdge(Request)) return false;

    // Build NodeA Data
    FRailNodeID NodeA = Request.ExistingNodeA.IsValid()
        ? Request.ExistingNodeA
        : CreateNode(Request.TransformA);

    // Build NodeB Data
    FRailNodeID NodeB = Request.ExistingNodeB.IsValid()
        ? Request.ExistingNodeB
        : CreateNode(Request.TransformB);

    // Build tangent data for both ends
    FVector TanA, TanB;
    ResolveRequestTangents(Request, TanA, TanB);

    float Dist = FVector::Distance(
        Graph.GetNodeData(NodeA.Value)->Transform.GetLocation(),
        Graph.GetNodeData(NodeB.Value)->Transform.GetLocation());

    UE_LOG(LogTemp, Warning, TEXT("TanA: %s | TanB: %s"),
        *TanA.ToString(),
        *TanB.ToString())
    CreateEdge(NodeA, NodeB, TanA * Dist, TanB * Dist);
    return true;
}

bool URailNetworkSubsystem::CanRemoveEdge(FRailEdgeID EdgeID) const
{
    // Always valid for now — future: check for active trains, locked sections, etc.
    return RailEdges.Contains(EdgeID.Value);
}

bool URailNetworkSubsystem::RequestRemoveEdge(FRailEdgeID EdgeID)
{
    if (!CanRemoveEdge(EdgeID)) return false;
    return RemoveEdge(EdgeID);
}

bool URailNetworkSubsystem::CanMoveNode(FRailNodeID NodeID, const FTransform& ProposedTransform) const
{
    const FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node) return false;

    const FVector NewPos = ProposedTransform.GetLocation();

    // Check curve radius for all connected edges with the proposed position
    for (FRailEdgeID EdgeID : Node->ConnectedEdges)
    {
        const FRailEdgeData* Edge = RailEdges.Find(EdgeID.Value);
        if (!Edge) continue;

        bool bIsNodeA = Edge->NodeA.Value == NodeID.Value;
        FRailNodeID OtherNodeID = bIsNodeA ? Edge->NodeB : Edge->NodeA;
        const FNodeData* OtherNode = Graph.GetNodeData(OtherNodeID.Value);
        if (!OtherNode) continue;

        FVector OtherPos = OtherNode->Transform.GetLocation();
        float Dist = FVector::Distance(NewPos, OtherPos);

        // Recompute tangent from new proposed transform
        FVector NewForward = ProposedTransform.GetUnitAxis(EAxis::X);
        FVector NewTangent = NewForward * Dist;
        FVector OtherTangent = bIsNodeA ? Edge->TangentB : Edge->TangentA;

        if (bIsNodeA)
        {
            if (!CheckCurveRadius(NewPos, NewTangent, OtherPos, OtherTangent))
                return false;
        }
        else
        {
            if (!CheckCurveRadius(OtherPos, OtherTangent, NewPos, NewTangent))
                return false;
        }
    }

    return true;
}

bool URailNetworkSubsystem::RequestMoveNode(FRailNodeID NodeID, const FTransform& NewTransform)
{
    if (!CanMoveNode(NodeID, NewTransform)) return false;

    Graph.SetNodeTransform(NodeID.Value, NewTransform);
    OnNodeTransformChanged(NodeID);
    return true;
}

// ---------- INTERNAL HELPERS ----------

float URailNetworkSubsystem::GetSnappedEdgeAngle(FRailNodeID NodeID, const FVector& EdgeTangent) const
{
    const FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node || Node->EdgeAngles.Num() == 0)
        return ComputeEdgeAngleAtNode(NodeID, EdgeTangent);

    float RawAngle = ComputeEdgeAngleAtNode(NodeID, EdgeTangent);

    // Find closest existing family
    float ClosestRepAngle = RawAngle;
    float ClosestDiff = TNumericLimits<float>::Max();

    for (const auto& Pair : Node->EdgeAngles)
    {
        float Diff = FMath::Abs(RawAngle - Pair.Value);
        Diff = FMath::Min(Diff, 180.f - Diff);
        if (Diff < ClosestDiff)
        {
            ClosestDiff = Diff;
            ClosestRepAngle = Pair.Value;
        }
    }

    // If within family threshold, snap to family angle
    if (ClosestDiff <= 15.f)
        return ClosestRepAngle;

    // New family — use raw angle
    return RawAngle;
}

float URailNetworkSubsystem::ComputeEdgeAngleAtNode(FRailNodeID NodeID, const FVector& EdgeTangent) const
{
    const FNodeData* GraphNode = Graph.GetNodeData(NodeID.Value);
    if (!GraphNode) return 0.f;

    FVector Forward = GraphNode->Transform.GetUnitAxis(EAxis::X);
    FVector Up = GraphNode->Transform.GetUnitAxis(EAxis::Z);
    FVector TangentNorm = EdgeTangent.GetSafeNormal();

    // Project tangent onto the node's XY plane
    FVector TangentInPlane = (TangentNorm - Up * FVector::DotProduct(TangentNorm, Up)).GetSafeNormal();

    // Compute signed angle from forward in the plane
    float Angle = FMath::RadiansToDegrees(FMath::Atan2(
        FVector::DotProduct(FVector::CrossProduct(Forward, TangentInPlane), Up),
        FVector::DotProduct(Forward, TangentInPlane)
    ));

    return Angle;
}

void URailNetworkSubsystem::UpdateNodeType(FRailNodeID NodeID)
{
    FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node) return;

    const int32 EdgeCount = Node->ConnectedEdges.Num();

    if (EdgeCount == 0)
    {
        RemoveNode(NodeID);
        return;
    }

    if (EdgeCount == 1)
    {
        Node->Type = ERailNodeType::Control;
        return;
    }

    // Cluster edges into angle families by 15° threshold
    struct FFamily
    {
        float RepAngle;
        int32 Count;
    };
    TArray<FFamily> Families;

    for (const auto& Pair : Node->EdgeAngles)
    {
        bool bFound = false;
        for (FFamily& Family : Families)
        {
            float Diff = FMath::Abs(Pair.Value - Family.RepAngle);
            Diff = FMath::Min(Diff, 180.f - Diff);
            if (Diff <= 15.f)
            {
                Family.Count++;
                bFound = true;
                break;
            }
        }
        if (!bFound)
            Families.Add({ Pair.Value, 1 });
    }

    if (Families.Num() == 1)
    {
        if (EdgeCount == 2)
        {
            const FRailEdgeData* E0 = RailEdges.Find(Node->ConnectedEdges[0].Value);
            const FRailEdgeData* E1 = RailEdges.Find(Node->ConnectedEdges[1].Value);
            if (!E0 || !E1)
            {
                Node->Type = ERailNodeType::Control;
                return;
            }

            // Normalize both to "pointing away" from this node
            FVector T0 = (E0->NodeA.Value == NodeID.Value) ? E0->TangentA : -E0->TangentB;
            FVector T1 = (E1->NodeA.Value == NodeID.Value) ? E1->TangentA : -E1->TangentB;
            T0 = T0.GetSafeNormal();
            T1 = T1.GetSafeNormal();

            float Dot = FVector::DotProduct(T0, T1);

            if (Dot < 0.f)
            {
                // Opposing — through-route, Control
                Node->Type = ERailNodeType::Control;
            }
            else
            {
                // Same direction — branch, Switch (trunk needed)
                Node->Type = ERailNodeType::Switch;
            }
            return;
        }
        else
        {
            // 3+ edges, single family - Switch
            Node->Type = ERailNodeType::Switch;
            return;
        }
    }
    else
    {
        // Multiple angle families — Crossover
        Node->Type = ERailNodeType::Crossover;
    }
}

void URailNetworkSubsystem::RecomputeEdgeDerived(FRailEdgeData& EdgeData)
{
    const FNodeData* NA = Graph.GetNodeData(EdgeData.NodeA.Value);
    const FNodeData* NB = Graph.GetNodeData(EdgeData.NodeB.Value);
    if (!NA || !NB) { EdgeData.Length = 0.f; return; }

    FVector PosA = NA->Transform.GetLocation();
    FVector PosB = NB->Transform.GetLocation();
    float Dist = FVector::Distance(PosA, PosB);

    EdgeData.TangentA = NA->Transform.GetUnitAxis(EAxis::X) * Dist;
    EdgeData.TangentB = NB->Transform.GetUnitAxis(EAxis::X) * Dist;

    RecomputeEdgeLength(EdgeData);
}

void URailNetworkSubsystem::RecomputeEdgeLength(FRailEdgeData& EdgeData)
{
    const FNodeData* NA = Graph.GetNodeData(EdgeData.NodeA.Value);
    const FNodeData* NB = Graph.GetNodeData(EdgeData.NodeB.Value);
    if (!NA || !NB) { EdgeData.Length = 0.f; return; }

    FVector PosA = NA->Transform.GetLocation();
    FVector PosB = NB->Transform.GetLocation();

    const int32 NumSteps = 32;
    float Length = 0.f;
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

// ---------- SWITCH CONTROL ----------

void URailNetworkSubsystem::SetSwitchActiveEdge(FRailNodeID NodeID, FRailEdgeID EdgeID)
{
    if (FSwitchNodeData* Switch = Switches.Find(NodeID.Value))
        Switch->ActiveEdge = EdgeID;
}

// ---------- QUERIES ----------

FVector URailNetworkSubsystem::GetSnappedTangentForNode(FRailNodeID NodeID, const FVector& IntentTangent) const
{
    const FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node || Node->EdgeAngles.Num() == 0) return IntentTangent;

    float RawAngle = ComputeEdgeAngleAtNode(NodeID, IntentTangent);

    float ClosestRepAngle = RawAngle;
    float ClosestDiff = TNumericLimits<float>::Max();

    for (const auto& Pair : Node->EdgeAngles)
    {
        float Diff = FMath::Abs(RawAngle - Pair.Value);
        Diff = FMath::Min(Diff, 180.f - Diff);
        if (Diff < ClosestDiff)
        {
            ClosestDiff = Diff;
            ClosestRepAngle = Pair.Value;
        }
    }

    if (ClosestDiff > 15.f) return IntentTangent; // New family, no snap

    // Convert snapped angle back to a world tangent
    const FNodeData* GraphNode = Graph.GetNodeData(NodeID.Value);
    if (!GraphNode) return IntentTangent;

    FVector NodeForward = GraphNode->Transform.GetUnitAxis(EAxis::X);
    FVector NodeUp = GraphNode->Transform.GetUnitAxis(EAxis::Z);
    return FQuat(NodeUp, FMath::DegreesToRadians(ClosestRepAngle))
        .RotateVector(NodeForward)
        .GetSafeNormal();
}

bool URailNetworkSubsystem::GetNodeData(FRailNodeID Node, FRailNodeData& OutData) const
{
    const FRailNodeData* Data = RailNodes.Find(Node.Value);
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

bool URailNetworkSubsystem::GetEdgeData(FRailEdgeID Edge, FRailEdgeData& OutData) const
{
    const FRailEdgeData* Data = RailEdges.Find(Edge.Value);
    if (!Data) return false;
    OutData = *Data;
    return true;
}

float URailNetworkSubsystem::GetEdgeLength(FRailEdgeID Edge) const
{
    const FRailEdgeData* Data = RailEdges.Find(Edge.Value);
    return Data ? Data->Length : 0.f;
}

TArray<FRailEdgeID> URailNetworkSubsystem::GetConnectedEdges(FRailNodeID Node) const
{
    const FRailNodeData* Data = RailNodes.Find(Node.Value);
    return Data ? Data->ConnectedEdges : TArray<FRailEdgeID>();
}

FTransform URailNetworkSubsystem::GetNodeTransform(FRailNodeID NodeID) const
{
    const FNodeData* Data = Graph.GetNodeData(NodeID.Value);
    return Data ? Data->Transform : FTransform::Identity;
}

FRailNodeID URailNetworkSubsystem::FindNearestNode(const FVector& WorldPos, float MaxDistanceCm) const
{
    FRailNodeID BestID;
    float BestDistSq = FMath::Square(MaxDistanceCm);

    for (const auto& Pair : RailNodes)
    {
        const FNodeData* NodeData = Graph.GetNodeData(Pair.Key);
        if (!NodeData) continue;

        const float DistSq = FVector::DistSquared(NodeData->Transform.GetLocation(), WorldPos);
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestID = Pair.Value.ID;
        }
    }

    return BestID;
}

void URailNetworkSubsystem::OnNodeTransformChanged(FRailNodeID NodeID)
{
    FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node) return;

    const FNodeData* NodeGraphData = Graph.GetNodeData(NodeID.Value);
    if (!NodeGraphData) return;

    for (FRailEdgeID EdgeID : Node->ConnectedEdges)
    {
        FRailEdgeData* Edge = RailEdges.Find(EdgeID.Value);
        if (!Edge) continue;

        bool bIsNodeA = Edge->NodeA.Value == NodeID.Value;
        FRailNodeID OtherNodeID = bIsNodeA ? Edge->NodeB : Edge->NodeA;
        const FNodeData* OtherNodeData = Graph.GetNodeData(OtherNodeID.Value);
        if (!OtherNodeData) continue;

        // Use existing tangent as hint to preserve direction intent
        FVector HintDir = bIsNodeA ? Edge->TangentA.GetSafeNormal() : Edge->TangentB.GetSafeNormal();
        FVector NodeForward = NodeGraphData->Transform.GetUnitAxis(EAxis::X);
        FVector NewTangentDir = FVector::DotProduct(NodeForward, HintDir) >= 0.f ? NodeForward : -NodeForward;

        float Dist = FVector::Distance(NodeGraphData->Transform.GetLocation(), OtherNodeData->Transform.GetLocation());

        if (bIsNodeA)
            Edge->TangentA = NewTangentDir * Dist;
        else
            Edge->TangentB = NewTangentDir * Dist;

        RecomputeEdgeLength(*Edge);
    }
}

// ---------- GEOMETRY ----------

FTransform URailNetworkSubsystem::GetTransformAtDistance(FRailEdgeID Edge, float S) const
{
    const FRailEdgeData* E = RailEdges.Find(Edge.Value);
    if (!E) return FTransform::Identity;

    const FNodeData* NA = Graph.GetNodeData(E->NodeA.Value);
    const FNodeData* NB = Graph.GetNodeData(E->NodeB.Value);
    if (!NA || !NB) return FTransform::Identity;

    const float Len = FMath::Max(E->Length, 1.f);
    const float T = FMath::Clamp(S / Len, 0.f, 1.f);

    FVector PosA = NA->Transform.GetLocation();
    FVector PosB = NB->Transform.GetLocation();

    const FVector Pos = RailMath::EvalHermitePos(PosA, E->TangentA, PosB, E->TangentB, T);
    FVector Tangent = RailMath::EvalHermiteTangent(PosA, E->TangentA, PosB, E->TangentB, T).GetSafeNormal();

    const FVector WorldUp = FVector::UpVector;
    FVector Up = (FMath::Abs(FVector::DotProduct(Tangent, WorldUp)) > 0.99f) ? FVector::RightVector : WorldUp;
    const FVector Right = FVector::CrossProduct(Up, Tangent).GetSafeNormal();
    const FVector TrueUp = FVector::CrossProduct(Tangent, Right).GetSafeNormal();

    return FTransform(FRotationMatrix::MakeFromXZ(Tangent, TrueUp).Rotator(), Pos);
}

FVector URailNetworkSubsystem::GetTangentForEdgeAtNode(FRailNodeID NodeID, FRailEdgeID EdgeID) const
{
    const FRailNodeData* Node = RailNodes.Find(NodeID.Value);
    if (!Node) return FVector::ForwardVector;

    const FNodeData* GraphNode = Graph.GetNodeData(NodeID.Value);
    if (!GraphNode) return FVector::ForwardVector;

    const float* Angle = Node->EdgeAngles.Find(EdgeID);
    if (!Angle) return GraphNode->Transform.GetUnitAxis(EAxis::X);

    FVector Forward = GraphNode->Transform.GetUnitAxis(EAxis::X);
    FVector Up = GraphNode->Transform.GetUnitAxis(EAxis::Z);

    return FQuat(Up, FMath::DegreesToRadians(*Angle)).RotateVector(Forward).GetSafeNormal();
}

FVector URailNetworkSubsystem::GetContinuationTangent(FRailNodeID NodeID, const FVector& HintDirection) const
{
    const FNodeData* NodeGraphData = Graph.GetNodeData(NodeID.Value);
    if (!NodeGraphData) return HintDirection;

    FVector Forward = NodeGraphData->Transform.GetUnitAxis(EAxis::X);
    return FVector::DotProduct(Forward, HintDirection.GetSafeNormal()) >= 0.f ? Forward : -Forward;
}

bool URailNetworkSubsystem::FindClosestRailLocation(FVector WorldPos, FRailLocation& Out, float& OutDistSq) const
{
    const float SampleStep = 100.f;
    const float RefineStep = 20.f;
    const int32 RefineIterations = 5;

    bool bFound = false;
    float BestDistSq = TNumericLimits<float>::Max();
    FRailLocation BestLoc;

    for (const auto& Pair : RailEdges)
    {
        const FRailEdgeID EdgeID{ Pair.Key };
        const FRailEdgeData& Edge = Pair.Value;
        if (Edge.Length <= KINDA_SMALL_NUMBER) continue;

        float BestSOnEdge = 0.f;
        float LocalBestDist = TNumericLimits<float>::Max();

        for (float S = 0.f; S <= Edge.Length; S += SampleStep)
        {
            const float DistSq = FVector::DistSquared(GetTransformAtDistance(EdgeID, S).GetLocation(), WorldPos);
            if (DistSq < LocalBestDist) { LocalBestDist = DistSq; BestSOnEdge = S; }
        }

        float Step = RefineStep;
        float Center = BestSOnEdge;
        for (int32 i = 0; i < RefineIterations; ++i)
        {
            float Start = FMath::Max(0.f, Center - Step);
            float End = FMath::Min(Edge.Length, Center + Step);
            for (float S = Start; S <= End; S += Step)
            {
                const float DistSq = FVector::DistSquared(GetTransformAtDistance(EdgeID, S).GetLocation(), WorldPos);
                if (DistSq < LocalBestDist) { LocalBestDist = DistSq; Center = S; }
            }
            Step *= 0.5f;
        }

        if (LocalBestDist < BestDistSq)
        {
            BestDistSq = LocalBestDist;
            BestLoc.Edge = EdgeID;
            BestLoc.S = Center;
            bFound = true;
        }
    }

    if (bFound) { Out = BestLoc; OutDistSq = BestDistSq; }
    return bFound;
}

// ---------- CONSTRAINT SOLVER ----------

bool URailNetworkSubsystem::GetPositionAndTangent(const FRailLocation& Loc, FVector& OutPos, FVector& OutTangent) const
{
    // Stub
    return true;
}

bool URailNetworkSubsystem::SolveTrailingForLinearDistance(const FRailLocation& Anchor, const FVector& AnchorPos, float TargetDist, const FRailLocation& InitialGuess, const FRailMoveContext& Ctx, FRailLocation& OutSolved, int32 MaxNewtonIters, float ToleranceCm)
{
    const float TargetDist2 = FMath::Square(TargetDist);
    const float Tol2 = FMath::Square(ToleranceCm);

    float Lo = 0.f;
    float Hi = TargetDist * 1.5f;

    for (int32 i = 0; i < MaxNewtonIters; ++i)
    {
        const float Mid = (Lo + Hi) * 0.5f;
        FRailTravelResult R = AdvanceAlongRails(Anchor, -Mid, Ctx);
        const float Dist2 = FVector::DistSquared(GetTransformAtDistance(R.RailLoc.Edge, R.RailLoc.S).GetLocation(), AnchorPos);

        if (Dist2 < TargetDist2) Lo = Mid;
        else Hi = Mid;

        if (FMath::Abs(Dist2 - TargetDist2) < Tol2 * TargetDist * 2.f) break;
    }

    FRailTravelResult Final = AdvanceAlongRails(Anchor, -(Lo + Hi) * 0.5f, Ctx);
    OutSolved = Final.RailLoc;
    return true;
}

// ---------- MOVEMENT ----------

FRailTravelResult URailNetworkSubsystem::AdvanceAlongRails(FRailLocation Location, float DeltaS, const FRailMoveContext& Ctx) const
{
    FRailTravelResult Result;
    Result.RailLoc = Location;

    float Remaining = FMath::Abs(DeltaS);
    if (DeltaS < 0.f)
        Result.RailLoc.Dir = Opposite(Location.Dir);

    while (Remaining > KINDA_SMALL_NUMBER)
    {
        const FRailEdgeData* E = RailEdges.Find(Result.RailLoc.Edge.Value);
        if (!E) { Result.bStopped = true; Result.StopReason = ERailStopReason::InvalidGraph; return Result; }

        const float DistToEnd = (Result.RailLoc.Dir == ERailDirection::AToB)
            ? (E->Length - Result.RailLoc.S)
            : Result.RailLoc.S;

        if (Remaining < DistToEnd)
        {
            Result.RailLoc.S += (Result.RailLoc.Dir == ERailDirection::AToB) ? Remaining : -Remaining;
            return Result;
        }

        Remaining -= DistToEnd;
        if (Remaining <= KINDA_SMALL_NUMBER) return Result;

        Result.RailLoc.S = (Result.RailLoc.Dir == ERailDirection::AToB) ? E->Length : 0.f;
        FRailNodeID ArriveNode = (Result.RailLoc.Dir == ERailDirection::AToB) ? E->NodeB : E->NodeA;
        Result.LastTransitionNode = ArriveNode;

        FRailEdgeID NextEdge = SelectNextEdge(ArriveNode, Result.RailLoc.Edge, Result.RailLoc.Dir, Ctx);
        if (!NextEdge.IsValid()) { Result.bStopped = true; Result.StopReason = ERailStopReason::NoNextEdge; return Result; }

        if (Ctx.bEnforceSignals && !CanEnterEdge(ArriveNode, NextEdge, 0))
        {
            Result.bStopped = true;
            Result.StopReason = ERailStopReason::BlockedBySignal;
            return Result;
        }

        const FRailEdgeData* NE = RailEdges.Find(NextEdge.Value);
        if (!NE) { Result.bStopped = true; Result.StopReason = ERailStopReason::InvalidGraph; return Result; }

        Result.RailLoc.Edge = NextEdge;

        if (NE->NodeA.Value == ArriveNode.Value)
        {
            Result.RailLoc.Dir = ERailDirection::AToB;
            Result.RailLoc.S = 0.f;
        }
        else if (NE->NodeB.Value == ArriveNode.Value)
        {
            Result.RailLoc.Dir = ERailDirection::BToA;
            Result.RailLoc.S = NE->Length;
        }
        else
        {
            Result.bStopped = true;
            Result.StopReason = ERailStopReason::InvalidGraph;
            return Result;
        }
    }

    return Result;
}

FRailEdgeID URailNetworkSubsystem::SelectNextEdge(FRailNodeID AtNode, FRailEdgeID IncomingEdge, ERailDirection IncomingDir, const FRailMoveContext& Ctx) const
{
    const FRailNodeData* Node = RailNodes.Find(AtNode.Value);
    if (!Node) return FRailEdgeID();

    TArray<FRailEdgeID> Candidates;
    for (const FRailEdgeID& E : Node->ConnectedEdges)
    {
        if (E.Value != IncomingEdge.Value || Ctx.bAllowUTurn)
            Candidates.Add(E);
    }

    if (Candidates.Num() == 0) return FRailEdgeID();

    // Planned path
    if (Ctx.bUsePlannedPath && Ctx.PlannedEdges.IsValidIndex(Ctx.PlannedIndex))
    {
        const FRailEdgeID PlannedNext = Ctx.PlannedEdges[Ctx.PlannedIndex];
        for (const FRailEdgeID& Candidate : Candidates)
            if (Candidate.Value == PlannedNext.Value) return Candidate;
        return FRailEdgeID();
    }

    // Switch
    if (Node->Type == ERailNodeType::Switch)
    {
        if (const FSwitchNodeData* Switch = Switches.Find(AtNode.Value))
        {
            if (Switch->ActiveEdge.IsValid())
            {
                for (const FRailEdgeID& Candidate : Candidates)
                    if (Candidate.Value == Switch->ActiveEdge.Value) return Candidate;
                return FRailEdgeID();
            }
        }
    }

    //// Crossover
    //if (Node->Type == ERailNodeType::Crossover)
    //{
    //    if (const FCrossoverNodeData* Crossover = Crossovers.Find(AtNode.Value))
    //    {
    //        if (const FRailEdgeID* Exit = Crossover->PairMap.Find(IncomingEdge))
    //            return *Exit;
    //        return FRailEdgeID();
    //    }
    //}

    // Single candidate fallback
    if (Candidates.Num() == 1) return Candidates[0];

    // Ambiguous
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
    for (const FRailEdgeID& E : InEdges)
        EdgeToBlock.Add(E.Value, NewID.Value);

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

void URailNetworkSubsystem::SaveNetwork(const FString& SlotName)
{
    UE_LOG(LogTemp, Warning, TEXT("SaveNetwork called"));
    URailNetworkSaveGame* SaveObject = Cast<URailNetworkSaveGame>(
        UGameplayStatics::CreateSaveGameObject(URailNetworkSaveGame::StaticClass()));
    if (!SaveObject) return;

    SaveObject->RailNodes = RailNodes;
    SaveObject->RailEdges = RailEdges;
    SaveObject->Switches = Switches;
    SaveObject->Blocks = Blocks;
    SaveObject->Signals = Signals;
    SaveObject->NextBlockID = NextBlockID;
    SaveObject->NextSignalID = NextSignalID;

    UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, 0);
    UE_LOG(LogTemp, Warning, TEXT("RailNetwork: Saved to slot '%s'"), *SlotName);
}

void URailNetworkSubsystem::LoadNetwork(const FString& SlotName)
{
    UE_LOG(LogTemp, Warning, TEXT("LoadNetwork called"));
    URailNetworkSaveGame* SaveObject = Cast<URailNetworkSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!SaveObject)
    {
        UE_LOG(LogTemp, Warning, TEXT("RailNetwork: No save found in slot '%s'"), *SlotName);
        return;
    }

    // Restore subsystem maps
    RailNodes = SaveObject->RailNodes;
    RailEdges = SaveObject->RailEdges;
    Switches = SaveObject->Switches;
    Blocks = SaveObject->Blocks;
    Signals = SaveObject->Signals;
    NextBlockID = SaveObject->NextBlockID;
    NextSignalID = SaveObject->NextSignalID;

    // Reconstruct FNetworkGraph from restored data
    Graph = FNetworkGraph();
    for (const auto& Pair : RailNodes)
    {
        Graph.AddNodeWithID(Pair.Key, Pair.Value.WorldTransform);
    }
    for (const auto& Pair : RailEdges)
    {
        Graph.AddEdgeWithID(Pair.Key, Pair.Value.NodeA.Value, Pair.Value.NodeB.Value);
    }

    // Rebuild EdgeToBlock lookup
    EdgeToBlock.Empty();
    for (const auto& Pair : Blocks)
    {
        for (const FRailEdgeID& EdgeID : Pair.Value.MemberEdges)
        {
            EdgeToBlock.Add(EdgeID.Value, Pair.Key);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("RailNetwork: Loaded from slot '%s' — %d nodes, %d edges"),
        *SlotName, RailNodes.Num(), RailEdges.Num());
}

// ---------- DEBUG ----------

void URailNetworkSubsystem::PrintNetworkData() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== RAIL NETWORK DATA ==="));
    UE_LOG(LogTemp, Warning, TEXT("Nodes: %d"), RailNodes.Num());

    for (const auto& Pair : RailNodes)
    {
        const FRailNodeData& Node = Pair.Value;
        const FNodeData* GraphNode = Graph.GetNodeData(Pair.Key);
        FVector Pos = GraphNode ? GraphNode->Transform.GetLocation() : FVector::ZeroVector;
        FVector Forward = GraphNode ? GraphNode->Transform.GetUnitAxis(EAxis::X) : FVector::ForwardVector;

        UE_LOG(LogTemp, Warning, TEXT("  Node %d | Type=%d | Pos=%s | Forward=%s | Angles=%d | Edges=%d"),
            Node.ID.Value,
            (int32)Node.Type,
            *Pos.ToString(),
            *Forward.ToString(),
            Node.EdgeAngles.Num(),
            Node.ConnectedEdges.Num());

        for (const auto& EdgeAngles : Node.EdgeAngles)
        {
            UE_LOG(LogTemp, Warning, TEXT("    -> Edge %d : %f Degrees"),
                EdgeAngles.Key.Value,
                EdgeAngles.Value);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Edges: %d"), RailEdges.Num());

    for (const auto& Pair : RailEdges)
    {
        const FRailEdgeData& Edge = Pair.Value;
        UE_LOG(LogTemp, Warning, TEXT("  Edge %d | A=%d B=%d | TanA=%s | TanB=%s | Len=%.1f"),
            Edge.ID.Value,
            Edge.NodeA.Value,
            Edge.NodeB.Value,
            *Edge.TangentA.ToString(),
            *Edge.TangentB.ToString(),
            Edge.Length);
    }

    UE_LOG(LogTemp, Warning, TEXT("========================="));
}

void URailNetworkSubsystem::ClearDebugDraw()
{
    FlushPersistentDebugLines(GetWorld());
}

void URailNetworkSubsystem::DebugDrawRailNetwork(float Duration, float Thickness) const
{
    UWorld* World = GetWorld();

    for (const auto& Pair : RailNodes)
    {
        const FNodeData* NodeData = Graph.GetNodeData(Pair.Key);
        if (!NodeData) continue;

        FLinearColor NodeColor = FLinearColor::Yellow;
        switch (Pair.Value.Type)
        {
        case ERailNodeType::Switch:    NodeColor = FLinearColor::Green; break;
        case ERailNodeType::Crossover: NodeColor = FLinearColor(1.f, 0.5f, 0.f); break; // Orange
        case ERailNodeType::Terminal:  NodeColor = FLinearColor::Red; break;
        default: break;
        }

        DrawDebugSphere(World, NodeData->Transform.GetLocation(), 20.f, 12, NodeColor.ToFColor(true), false, Duration, 0, 2.f);
    }

    for (const auto& Pair : RailEdges)
    {
        const FRailEdgeData& Edge = Pair.Value;
        const FNodeData* NA = Graph.GetNodeData(Edge.NodeA.Value);
        const FNodeData* NB = Graph.GetNodeData(Edge.NodeB.Value);
        if (!NA || !NB) continue;

        FVector PosA = NA->Transform.GetLocation();
        FVector PosB = NB->Transform.GetLocation();
        FVector Prev = PosA;

        for (int32 i = 1; i <= 32; ++i)
        {
            const float T = (float)i / 32.f;
            FVector Curr = RailMath::EvalHermitePos(PosA, Edge.TangentA, PosB, Edge.TangentB, T);
            DrawDebugLine(World, Prev, Curr, FColor::Cyan, false, Duration, 0, Thickness);
            Prev = Curr;
        }

        DrawDebugLine(World, PosA, PosA + Edge.TangentA, FColor::Green, false, Duration, 0, 2.f);
        DrawDebugLine(World, PosB, PosB + Edge.TangentB, FColor::Red, false, Duration, 0, 2.f);
    }
}

void URailNetworkSubsystem::DrawWithPDI(FPrimitiveDrawInterface* PDI) const
{
    for (const auto& Pair : RailNodes)
    {
        const FNodeData* NodeData = Graph.GetNodeData(Pair.Key);
        if (!NodeData) continue;
        PDI->DrawPoint(NodeData->Transform.GetLocation(), FLinearColor::Yellow, 10.f, SDPG_Foreground);
    }

    for (const auto& Pair : RailEdges)
    {
        const FRailEdgeData& Edge = Pair.Value;
        const FNodeData* NA = Graph.GetNodeData(Edge.NodeA.Value);
        const FNodeData* NB = Graph.GetNodeData(Edge.NodeB.Value);
        if (!NA || !NB) continue;

        FVector PosA = NA->Transform.GetLocation();
        FVector PosB = NB->Transform.GetLocation();
        FVector Prev = PosA;

        for (int32 i = 1; i <= 32; ++i)
        {
            const float T = (float)i / 32.f;
            FVector Curr = RailMath::EvalHermitePos(PosA, Edge.TangentA, PosB, Edge.TangentB, T);
            PDI->DrawLine(Prev, Curr, FLinearColor::Blue, SDPG_Foreground, 2.f);
            Prev = Curr;
        }
    }
}