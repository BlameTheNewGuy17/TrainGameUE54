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

	// ---------- CORE GRAPH ----------

	UFUNCTION(BlueprintCallable) FRailNodeID CreateNode(const FVector& WorldPos, ERailNodeType Type);
	UFUNCTION(BlueprintCallable) FRailEdgeID CreateEdge(FRailNodeID A, FRailNodeID B, const FVector& TangentA, const FVector& TangentB);

	UFUNCTION(BlueprintCallable) bool RemoveEdge(FRailEdgeID Edge);

	UFUNCTION(BlueprintPure) bool GetNodeData(FRailNodeID Node, FRailNodeData& OutData) const;
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

	// ---------- MOVEMENT ----------

	UFUNCTION(BlueprintCallable)
	FRailTravelResult AdvanceAlongRails(
		FRailEdgeID Edge,
		float S,
		ERailDirection Dir,
		float DeltaS,
		const FRailMoveContext& Ctx);

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
