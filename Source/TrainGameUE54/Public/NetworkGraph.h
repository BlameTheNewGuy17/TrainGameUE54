#pragma once
#include "CoreMinimal.h"


struct FNodeData
{
    FTransform Transform;
    TArray<int32> ConnectedEdges;
};

struct FEdgeData
{
    int32 NodeA = -1;
    int32 NodeB = -1;
};

class FNetworkGraph
{
public:
	int32 AddNode(const FTransform& Transform);
    int32 AddNodeWithID(int32 ID, const FTransform& Transform);
    const FNodeData* GetNodeData(int32 NodeID) const;
    void SetNodeTransform(int32 NodeID, const FTransform& NewTransform);
    bool RemoveNode(const int32 NodeID);

    int32 AddEdge(int32 A, int32 B);
    int32 AddEdgeWithID(int32 ID, int32 A, int32 B);
    const FEdgeData* GetEdgeData(const int32 EdgeID) const;
    bool RemoveEdge(const int32 EdgeID);

    TArray<int32> FindPath(int32 IDFrom, int32 IDTo, TFunction<bool(int32)> Filter = nullptr, TFunction<float(int32)> Cost = nullptr, TFunction<float(int32)> Heuristic = nullptr);

private:
    TMap<int32, FNodeData> Nodes;
    TMap<int32, FEdgeData> Edges;
    int32 NextNodeID = 1;
    int32 NextEdgeID = 1;
};