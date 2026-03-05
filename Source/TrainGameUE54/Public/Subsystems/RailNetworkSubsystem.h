// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RailNetworkTypes.h"
#include "RailNetworkSubsystem.generated.h"

/*
URailNetworkSubsystem
Authoritative rail simulation core (data only, no actors, no splines).
All geometry, routing, signaling, and saving flow through this class.

Multiplayer note:
This subsystem does NOT replicate.
In multiplayer, a replicated RailNetworkStateActor will forward commands here.
*/

UCLASS(BlueprintType)
class TRAINGAMEUE54_API URailNetworkSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ---------- DEBUG ----------

	UFUNCTION(BlueprintCallable)
	void ClearDebugDraw();
	UFUNCTION(BlueprintCallable)
	void DebugDrawRailNetwork(float Duration = 0.f, float Thickness = 2.f) const;

	void DrawWithPDI(FPrimitiveDrawInterface* PDI) const;

	// ---------- CORE GRAPH ----------

	UFUNCTION(BlueprintCallable) FRailNodeID CreateNode(const FTransform& WorldTransform, ERailNodeType Type);
	UFUNCTION(BlueprintCallable) FRailNodeID CreateSwitchNode(const FTransform& WorldTransform, const FSwitchNodeData& Data);
	UFUNCTION(BlueprintCallable) FRailNodeID CreateCrossoverNode(const FTransform& WorldTransform, const FCrossoverNodeData& Data);
	UFUNCTION(BlueprintCallable) FRailEdgeID CreateEdge(FRailNodeID A, FRailNodeID B);

	UFUNCTION(BlueprintCallable) void SetSwitchActiveEdge(FRailNodeID NodeID, FRailEdgeID EdgeID);

	UFUNCTION(BlueprintCallable) bool RemoveEdge(FRailEdgeID Edge);

	UFUNCTION(BlueprintPure) bool GetNodeData(FRailNodeID Node, FRailNodeData& OutData) const;
	UFUNCTION(BlueprintPure) bool GetSwitchData(FRailNodeID Node, FSwitchNodeData& OutData) const;
	UFUNCTION(BlueprintPure) bool GetCrossoverData(FRailNodeID Node, FCrossoverNodeData& OutData) const;

	UFUNCTION(BlueprintPure) bool GetEdgeData(FRailEdgeID Edge, FRailEdgeData& OutData) const;

	UFUNCTION(BlueprintPure) float GetEdgeLength(FRailEdgeID Edge) const;
	UFUNCTION(BlueprintPure) TArray<FRailEdgeID> GetConnectedEdges(FRailNodeID Node) const;

	// ---------- GEOMETRY (MOST IMPORTANT API) ----------

	/*
	GetTransformAtDistance
	The single authoritative geometry query for all trains.
	Must match visual spline generation exactly.
	*/
	UFUNCTION(BlueprintPure)
	FTransform GetTransformAtDistance(FRailEdgeID Edge, float S) const;

	UFUNCTION(BlueprintPure)
	FVector GetTangentForEdgeAtNode(FRailNodeID NodeID, FRailEdgeID EdgeID) const;

	UFUNCTION(BlueprintPure)
	bool FindClosestRailLocation(FVector WorldPos, FRailLocation& Out, float& OutDistSq) const;

	UFUNCTION(BlueprintPure)
	FRailNodeID FindNearestNode(const FVector& WorldPos, float MaxDistanceCm) const;

	// ---------- CONSTRAINT SOLVER ----------

	UFUNCTION(BlueprintPure)
	bool GetPositionAndTangent(
		const FRailLocation& Loc,
		FVector& OutPos,
		FVector& OutTangent
	) const;

	/*
	Solve for a trailing rail location such that the WORLD distance
	to AnchorPos is exactly TargetDist (meters).
	
	This is the "no accordion" constraint.
	*/
	UFUNCTION(BlueprintCallable)
	bool SolveTrailingForLinearDistance(
		const FRailLocation& Anchor,
		const FVector& AnchorPos,
		float TargetDist,
		const FRailLocation& InitialGuess,
		const FRailMoveContext& Ctx,
		FRailLocation& OutSolved,
		int32 MaxNewtonIters = 12,
		float ToleranceCm = 0.5f
	);

	// ---------- MOVEMENT ----------

	UFUNCTION(BlueprintCallable)
	FRailTravelResult AdvanceAlongRails(
		FRailLocation Location,
		float DeltaS,
		const FRailMoveContext& Ctx) const;

	UFUNCTION(BlueprintCallable)
	FRailEdgeID SelectNextEdge(
		FRailNodeID AtNode,
		FRailEdgeID IncomingEdge,
		ERailDirection IncomingDir,
		const FRailMoveContext& Ctx) const;

	// ---------- BLOCKS ----------

	UFUNCTION(BlueprintCallable)
	FRailBlockID CreateBlock(FName Label, ERailBlockType Type, const TArray<FRailEdgeID>& Edges);

	UFUNCTION(BlueprintPure)
	FRailBlockID GetBlockForEdge(FRailEdgeID Edge) const;

	UFUNCTION(BlueprintPure)
	bool IsBlockOccupied(FRailBlockID Block) const;

	// ---------- SIGNALS ----------

	UFUNCTION(BlueprintCallable)
	FRailSignalID CreateSignal(FRailNodeID Node, FRailEdgeID Edge, FRailBlockID Block);

	UFUNCTION(BlueprintPure)
	bool CanEnterEdge(FRailNodeID AtNode, FRailEdgeID NextEdge, int32 TrainID) const;

private:

	// Authoritative storage
	UPROPERTY(SaveGame) TMap<int32, FRailNodeData> Nodes;
	UPROPERTY(SaveGame) TMap<int32, FSwitchNodeData> Switches;
	UPROPERTY(SaveGame) TMap<int32, FCrossoverNodeData> Crossovers;
	UPROPERTY(SaveGame) TMap<int32, FRailEdgeData> Edges;
	UPROPERTY(SaveGame) TMap<int32, FRailBlockData> Blocks;
	UPROPERTY(SaveGame) TMap<int32, FRailSignalData> Signals;

	// Runtime caches
	UPROPERTY(Transient) TMap<int32, int32> EdgeToBlock;

	// ID generators
	int32 NextNodeID = 1;
	int32 NextEdgeID = 1;
	int32 NextBlockID = 1;
	int32 NextSignalID = 1;

	// Internal helpers
	void RecomputeEdgeDerived(FRailEdgeData& EdgeData);
};
