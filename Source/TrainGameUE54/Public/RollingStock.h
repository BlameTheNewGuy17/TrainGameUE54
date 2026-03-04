// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RailPhysicsTypes.h"
#include "RailNetworkTypes.h"
#include "RollingStockTypes.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "RollingStock.generated.h"

UCLASS()
class TRAINGAMEUE54_API ARollingStock : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARollingStock();
	void InitializeRollingStock(FRollingStockID InID, int32 NumBogies, const FRailConstraintProfile& InProfile);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Returns the current state of this rolling stock actor's constraint profile. 
	const FRailConstraintProfile& GetRailConstraintProfile() const;
	// Updates the current state of the rolling stock actor's constraint profile.
	void SetRailConstraintProfile(const FRailConstraintProfile& NewProfile);
	
	// Fills the supplied array with FRailLocations for each bogie. Supports articulated cars or locomotives.
	void GetBogieLocations(TArray<FRailLocation>& OutLocations) const;
	// Allows us to fill the locations in bulk
	void SetBogieLocations(const TArray<FRailLocation>& InLocations);
	// Asks the RailNetwork for current locations for each of our bogies.
	void UpdateBogieLocationsFromNetwork();

	// Returns this rolling stock actors ID.
	FRollingStockID GetRollingStockID() const;
	// We don't have a SetRollingStockID as it should only be set at creation.  

private:

	bool bInitialized = false;

	UPROPERTY(EditAnywhere, Category="Physics")
	FRailConstraintProfile Profile;

	UPROPERTY(SaveGame)
	TArray<FRailLocation> BogieLocations;
	
	UPROPERTY(VisibleAnywhere)
	FRollingStockID ID;
	
	UPROPERTY()
	URailNetworkSubsystem* RailNetwork = nullptr;
	
	UPROPERTY(VisibleAnywhere)
	bool bDerailed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics", meta = (AllowPrivateAccess = "true"))
	UPrimitiveComponent* PhysicsRoot;


};
