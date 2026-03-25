// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RailNetworkTypes.h"
#include "RollingStockTypes.h"
#include "RailwayPhysicsCallback.h"
#include "TrainSimulationSubsystem.generated.h"


// Forward declarations
class FRailwayPhysicsCallback;
class URailNetworkSubsystem;

/**
 * 
 */
UCLASS()
class TRAINGAMEUE54_API UTrainSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ---------- DEBUG ----------
	UFUNCTION(BlueprintCallable)
	void DebugDrawRollingStock(float Duration = 0.f, float Thickness = 2.f) const;

	// ---------- CORE ---------
	UFUNCTION(BlueprintCallable)
	FRollingStockID AddRollingStock(URollingStockDefinition* RollingStockDefinition, FRailLocation Location);

	UFUNCTION(BlueprintCallable)
	bool RemoveRollingStock(FRollingStockID ID);

	UFUNCTION(BlueprintCallable)
	void CoupleCars(FRollingStockID CarA, FRollingStockID CarB, FTrainID& TrainOut);

	UFUNCTION(BlueprintCallable)
	void UncoupleCars(FRollingStockID CarA, FRollingStockID CarB, FTrainID& TrainAOut, FTrainID& TrainBOut);

	void AdvanceTrain(FTrainData Train, float DeltaTime);
	void AdvanceRollingStock(FRollingStockID ID, float DeltaTime, FRailLocation& SolvedRailLocOut);

private:
	TMap<FRollingStockID, FRollingStockState> RollingStockStates;
	TMap<FTrainID, FTrainData> Trains;

	// ID generators
	int32 NextRollingStockID = 1;
	int32 NextTrainID = 1;

	URailNetworkSubsystem* RailNetworkRef = nullptr;

	// Physics

	bool bUsePhysics = false;

	FRailwayPhysicsCallback* RailCallback = nullptr;

	TArray<AActor*> PhysActorRefs;
	TMap<FRollingStockID, TWeakObjectPtr<AActor>> IDToActor;
	TMap<FRollingStockID, FTrackedRailBody> CachedTrackedBodies;

	TMap<FRollingStockID, FTrackedRailBody> RailBodyRegistry;

	bool bLogDebug = false; // Debug stuff like draw, positions, etc.
	bool bLogSetup = false; // Init, BeginPlay, etc.
	bool bLogModification = true; // Data storage modification like de/registration
	bool bLogMovement = true; // Tick, Advance Rolling Stock

};
