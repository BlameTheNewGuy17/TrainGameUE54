#include "NetworkGraph.h"
#include "NetworkGraph.h"

int32 FNetworkGraph::AddNode(const FTransform& Transform)
{
    int32 NewID = NextNodeID++;
    FNodeData Data;
    Data.Transform = Transform;
    Nodes.Add(NewID, Data);
    return NewID;
}

int32 FNetworkGraph::AddNodeWithID(int32 ID, const FTransform& Transform)
{
    FNodeData Data;
    Data.Transform = Transform;
    Nodes.Add(ID, Data);
    NextNodeID = FMath::Max(NextNodeID, ID + 1);
    return ID;
}

const FNodeData* FNetworkGraph::GetNodeData(int32 NodeID) const
{
    return Nodes.Find(NodeID);
}

void FNetworkGraph::SetNodeTransform(int32 NodeID, const FTransform& NewTransform)
{
    if (FNodeData* Node = Nodes.Find(NodeID))
    {
        Node->Transform = NewTransform;
    }
}

bool FNetworkGraph::RemoveNode(const int32 NodeID)
{
    const FNodeData* Data = Nodes.Find(NodeID);
    if (!Data) return false;

    // Remove all connected edges first
    TArray<int32> EdgesToRemove = Data->ConnectedEdges;
    for (int32 EdgeID : EdgesToRemove)
    {
        RemoveEdge(EdgeID);
    }

    Nodes.Remove(NodeID);
    return true;
}

int32 FNetworkGraph::AddEdge(int32 A, int32 B)
{
    if (!Nodes.Contains(A) || !Nodes.Contains(B)) return -1;

    int32 NewID = NextEdgeID++;
    FEdgeData Data;
    Data.NodeA = A;
    Data.NodeB = B;
    Edges.Add(NewID, Data);

    Nodes[A].ConnectedEdges.Add(NewID);
    Nodes[B].ConnectedEdges.Add(NewID);

    return NewID;
}

int32 FNetworkGraph::AddEdgeWithID(int32 ID, int32 A, int32 B)
{
    FEdgeData Data;
    Data.NodeA = A;
    Data.NodeB = B;
    Edges.Add(ID, Data);

    // Restore connectivity
    if (FNodeData* NA = Nodes.Find(A)) NA->ConnectedEdges.Add(ID);
    if (FNodeData* NB = Nodes.Find(B)) NB->ConnectedEdges.Add(ID);

    NextEdgeID = FMath::Max(NextEdgeID, ID + 1);
    return ID;
}

const FEdgeData* FNetworkGraph::GetEdgeData(const int32 EdgeID) const
{
    return Edges.Find(EdgeID);
}

bool FNetworkGraph::RemoveEdge(const int32 EdgeID)
{
    const FEdgeData* Data = Edges.Find(EdgeID);
    if (!Data) return false;

    if (FNodeData* NA = Nodes.Find(Data->NodeA)) NA->ConnectedEdges.Remove(EdgeID);
    if (FNodeData* NB = Nodes.Find(Data->NodeB)) NB->ConnectedEdges.Remove(EdgeID);

    Edges.Remove(EdgeID);
    return true;
}

TArray<int32> FNetworkGraph::FindPath(int32 IDFrom, int32 IDTo, TFunction<bool(int32)> Filter, TFunction<float(int32)> Cost, TFunction<float(int32)> Heuristic)
{
    TMap<int32, float> CostSoFar;
    TMap<int32, int32> CameFromNode;
    TMap<int32, int32> CameFromEdge;
    TArray<int32> Result;

    if (!Nodes.Contains(IDFrom) || !Nodes.Contains(IDTo)) return Result;

    TArray<TPair<float, int32>> Open;
    Open.Add({ 0.f, IDFrom });
    CostSoFar.Add(IDFrom, 0.f);

    while (Open.Num() > 0)
    {
        Open.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
            {
                return A.Key < B.Key;
            });
        TPair<float, int32> Current = Open[0];
        Open.RemoveAt(0);

        int32 CurrentNode = Current.Value;

        if (CurrentNode == IDTo)
        {
            int32 Step = IDTo;
            while (CameFromEdge.Contains(Step))
            {
                Result.Insert(CameFromEdge[Step], 0);
                Step = CameFromNode[Step];
            }
            return Result;
        }

        const FNodeData* NodeData = Nodes.Find(CurrentNode);
        if (!NodeData) continue;

        for (int32 EdgeID : NodeData->ConnectedEdges)
        {
            if (Filter && !Filter(EdgeID)) continue;

            const FEdgeData* EdgeData = Edges.Find(EdgeID);
            if (!EdgeData) continue;

            int32 Neighbor = (EdgeData->NodeA == CurrentNode) ? EdgeData->NodeB : EdgeData->NodeA;

            float EdgeCost = Cost ? Cost(EdgeID) : 1.f;
            float NewCost = CostSoFar[CurrentNode] + EdgeCost;

            if (!CostSoFar.Contains(Neighbor) || NewCost < CostSoFar[Neighbor])
            {
                CostSoFar.Add(Neighbor, NewCost);
                CameFromNode.Add(Neighbor, CurrentNode);
                CameFromEdge.Add(Neighbor, EdgeID);

                // A* adds heuristic to priority, Dijkstra just uses cost
                float Priority = NewCost + (Heuristic ? Heuristic(Neighbor) : 0.f);
                Open.Add({ Priority, Neighbor });
            }
        }
    }

    return Result;
}