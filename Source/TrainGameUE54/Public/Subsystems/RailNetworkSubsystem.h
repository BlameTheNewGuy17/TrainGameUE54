// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RailNetworkTypes.h"
#include "NetworkGraph.h"
#include "RailNetworkSubsystem.generated.h"

/*
URailNetworkSubsystem
Authoritative rail simulation core (data only, no actors, no splines).
All geometry, routing, signaling, and saving flow through this class.

Multiplayer note:
This subsystem does NOT replicate.
In multiplayer, a replicated RailNetworkStateActor will forward commands here.
*/


// -----------------------------------------------------------------------
// FEdgePlacementRequest
// Describes a desired edge placement. If ExistingNodeA/B are invalid,
// the subsystem will create new nodes using TransformA/B.
// -----------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FEdgePlacementRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FRailNodeID ExistingNodeA;
    UPROPERTY(BlueprintReadWrite) FRailNodeID ExistingNodeB;
    UPROPERTY(BlueprintReadWrite) FTransform TransformA;
    UPROPERTY(BlueprintReadWrite) FTransform TransformB;
    UPROPERTY(BlueprintReadWrite) FVector TangentA = FVector::ForwardVector;
    UPROPERTY(BlueprintReadWrite) FVector TangentB = FVector::ForwardVector;
};

UCLASS(BlueprintType)
class TRAINGAMEUE54_API URailNetworkSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // ---------- TUNING ----------

    // Minimum curve radius in cm. 0 = no limit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Network|Validation")
    float MinCurveRadiusCm = 200.f;

    // ---------- DEBUG ----------

    UFUNCTION(CallInEditor, Category = "Debug") void PrintNetworkData() const;
    UFUNCTION(BlueprintCallable) void ClearDebugDraw();
    UFUNCTION(BlueprintCallable) void DebugDrawRailNetwork(float Duration = 0.f, float Thickness = 2.f) const;
    void DrawWithPDI(FPrimitiveDrawInterface* PDI) const;

    // ---------- PLACEMENT API ----------

    UFUNCTION(BlueprintPure) bool CanPlaceEdge(const FEdgePlacementRequest& Request) const;
    UFUNCTION(BlueprintCallable) bool RequestPlaceEdge(const FEdgePlacementRequest& Request, FRailEdgeData& OutEdge, FRailNodeData& OutNodeA, FRailNodeData& OutNodeB);
    UFUNCTION(BlueprintPure) bool CanRemoveEdge(FRailEdgeID EdgeID) const;
    UFUNCTION(BlueprintCallable) bool RequestRemoveEdge(FRailEdgeID EdgeID);

    UFUNCTION(BlueprintPure) bool CanMoveNode(FRailNodeID NodeID, const FTransform& ProposedTransform) const;
    UFUNCTION(BlueprintCallable) bool RequestMoveNode(FRailNodeID NodeID, const FTransform& NewTransform);

    // ---------- SWITCH CONTROL ----------

    UFUNCTION(BlueprintCallable) void SetSwitchActiveEdge(FRailNodeID NodeID, FRailEdgeID EdgeID);

    // ---------- QUERIES ----------

    const TMap<int32, FRailNodeData>& GetNodes() const { return RailNodes; }

    UFUNCTION(BlueprintPure) FVector GetSnappedTangentForNode(FRailNodeID NodeID, const FVector& IntentTangent) const;
    UFUNCTION(BlueprintPure) bool GetNodeData(FRailNodeID Node, FRailNodeData& OutData) const;
    UFUNCTION(BlueprintPure) bool GetSwitchData(FRailNodeID Node, FSwitchNodeData& OutData) const;
    UFUNCTION(BlueprintPure) bool GetEdgeData(FRailEdgeID Edge, FRailEdgeData& OutData) const;
    UFUNCTION(BlueprintPure) float GetEdgeLength(FRailEdgeID Edge) const;
    UFUNCTION(BlueprintPure) TArray<FRailEdgeID> GetConnectedEdges(FRailNodeID Node) const;
    UFUNCTION(BlueprintPure) FTransform GetNodeTransform(FRailNodeID NodeID) const;
    UFUNCTION(BlueprintPure) FRailNodeID FindNearestNode(const FVector& WorldPos, float MaxDistanceCm) const;
    const TMap<int32, FRailEdgeData>& GetEdges() const { return RailEdges; }


    void OnNodeTransformChanged(FRailNodeID NodeID);

    // ---------- GEOMETRY ----------

    UFUNCTION(BlueprintPure) FTransform GetTransformAtDistance(FRailEdgeID Edge, float S) const;
    UFUNCTION(BlueprintPure) FVector GetTangentForEdgeAtNode(FRailNodeID NodeID, FRailEdgeID EdgeID) const;
    UFUNCTION(BlueprintPure) FVector GetContinuationTangent(FRailNodeID NodeID, const FVector& HintDirection) const;
    UFUNCTION(BlueprintPure) bool FindClosestRailLocation(FVector WorldPos, FRailLocation& Out, float& OutDistSq) const;

    // ---------- CONSTRAINT SOLVER ----------

    UFUNCTION(BlueprintPure) bool GetPositionAndTangent(const FRailLocation& Loc, FVector& OutPos, FVector& OutTangent) const;
    UFUNCTION(BlueprintCallable) bool SolveTrailingForLinearDistance(const FRailLocation& Anchor, const FVector& AnchorPos, float TargetDist, const FRailLocation& InitialGuess, const FRailMoveContext& Ctx, FRailLocation& OutSolved, int32 MaxNewtonIters = 12, float ToleranceCm = 0.5f);

    // ---------- MOVEMENT ----------

    UFUNCTION(BlueprintCallable) FRailTravelResult AdvanceAlongRails(FRailLocation Location, float DeltaS, const FRailMoveContext& Ctx) const;
    UFUNCTION(BlueprintCallable) FRailEdgeID SelectNextEdge(FRailNodeID AtNode, FRailEdgeID IncomingEdge, ERailDirection IncomingDir, const FRailMoveContext& Ctx) const;

    // ---------- BLOCKS ----------

    UFUNCTION(BlueprintCallable) FRailBlockID CreateBlock(FName Label, ERailBlockType Type, const TArray<FRailEdgeID>& Edges);
    UFUNCTION(BlueprintPure) FRailBlockID GetBlockForEdge(FRailEdgeID Edge) const;
    UFUNCTION(BlueprintPure) bool IsBlockOccupied(FRailBlockID Block) const;

    // ---------- SIGNALS ----------

    UFUNCTION(BlueprintCallable) FRailSignalID CreateSignal(FRailNodeID Node, FRailEdgeID Edge, FRailBlockID Block);
    UFUNCTION(BlueprintPure) bool CanEnterEdge(FRailNodeID AtNode, FRailEdgeID NextEdge, int32 TrainID) const;

    // ---------- SAVEGAME ----------

    UFUNCTION(CallInEditor) void SaveNetwork(const FString& SlotName = "Test");
    UFUNCTION(CallInEditor) void LoadNetwork(const FString& SlotName = "Test");

    // ---------- REPLICATION HELPERS ----------
    UFUNCTION(BlueprintCallable) void LoadSnapshot(const TArray<FRailNodeSnapshot>& Nodes, const TArray<FRailEdgeData>& Edges);
    UFUNCTION(BlueprintCallable) void ApplyRemoteEdgePlacement(const FRailEdgeData& Edge, const FRailNodeSnapshot& NodeA, const FRailNodeSnapshot& NodeB);
    UFUNCTION(BlueprintCallable) void ApplyRemoteEdgeRemoval(FRailEdgeID EdgeID);

private:

    // ---------- STORAGE ----------

    // Generic graph — owns transforms, connectivity, pathfinding
    FNetworkGraph Graph;

    // Rail-specific data, keyed by graph node/edge ID
    UPROPERTY(SaveGame) TMap<int32, FRailNodeData> RailNodes;
    UPROPERTY(SaveGame) TMap<int32, FSwitchNodeData> Switches;
    UPROPERTY(SaveGame) TMap<int32, FRailEdgeData> RailEdges;
    UPROPERTY(SaveGame) TMap<int32, FRailBlockData> Blocks;
    UPROPERTY(SaveGame) TMap<int32, FRailSignalData> Signals;
    UPROPERTY(Transient) TMap<int32, int32> EdgeToBlock;

    int32 NextBlockID = 1;
    int32 NextSignalID = 1;
    // Note: Core ID Gen (NextNodeID and NextEdgeID) live inside FNetworkGraph

    // ---------- INTERNAL GRAPH OPS ----------

    FRailNodeID CreateNode(const FTransform& WorldTransform);
    FRailEdgeID CreateEdge(FRailNodeID A, FRailNodeID B, const FVector& TangentA, const FVector& TangentB);
    bool RemoveEdge(FRailEdgeID EdgeID);
    bool RemoveNode(FRailNodeID NodeID);

    // ---------- INTERNAL VALIDATION ----------


    // Helper to get the snapped angle for storage
    bool ValidateEdgeRequest(const FEdgePlacementRequest& Request) const;
    void ResolveRequestTangents(const FEdgePlacementRequest& Request, FVector& OutTanA, FVector& OutTanB) const;
    bool CanAddEdgeToNode(FRailNodeID NodeID, const FVector& IncomingTangent) const;
    bool CheckCurveRadius(const FVector& PosA, const FVector& TanA, const FVector& PosB, const FVector& TanB) const;

    // ---------- INTERNAL HELPERS ----------

    float GetSnappedEdgeAngle(FRailNodeID NodeID, const FVector& EdgeTangent) const;
    float ComputeEdgeAngleAtNode(FRailNodeID NodeID, const FVector& EdgeTangent) const;
    void UpdateNodeType(FRailNodeID NodeID);
    void RecomputeEdgeDerived(FRailEdgeData& EdgeData);
    void RecomputeEdgeLength(FRailEdgeData& EdgeData);
};