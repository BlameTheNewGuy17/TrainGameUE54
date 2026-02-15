// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RailNetworkTypes.h"
#include "RollingStockTypes.h"
#include "TrainSimulationSubsystem.generated.h"


// Forward declare for the RailwayPhysicsCallback Class
class FRailwayPhysicsCallback;

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
	FRollingStockID AddRollingStock(ERollingStockType Type, FRailLocation Location);

	UFUNCTION(BlueprintCallable)
	bool RemoveRollingStock(FRollingStockID ID);

	void AdvanceRollingStock(FRollingStockID ID, float DeltaTime);

private:
	TMap<FRollingStockID, FRollingStockState> RollingStockStates;

	// ID generators
	int32 NextRollingStockID = 1;
	int32 NextTrainID = 1;

	// Physics
	FRailwayPhysicsCallback* RailCallback = nullptr;


};
