// Fill out your copyright notice in the Description page of Project Settings.


#include "RollingStock.h"

// Sets default values
ARollingStock::ARollingStock()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	PhysicsRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsRoot"));
	SetRootComponent(PhysicsRoot);
	PhysicsRoot->SetSimulatePhysics(true);

}

void ARollingStock::InitializeRollingStock(FRollingStockID InID, int32 NumBogies, const FRailConstraintProfile& InProfile)
{
	if (bInitialized)
	{
		ensureMsgf(false, TEXT("InitializeRollingStock called twice"));
		return;
	}

	bInitialized = true;
	ID = InID;
	Profile = InProfile;

	BogieLocations.Empty();
	BogieLocations.SetNum(NumBogies);
}

// Called when the game starts or when spawned
void ARollingStock::BeginPlay()
{
	Super::BeginPlay();
	RailNetwork = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
	
	ensureMsgf(PhysicsRoot, TEXT("PhysicsRoot missing"));
	ensureMsgf(RailNetwork, TEXT("RailNetwork subsystem missing"));
	ensureMsgf(BogieLocations.Num() > 0, TEXT("RollingStock has no bogies"));

}

// Called every frame
void ARollingStock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

const FRailConstraintProfile& ARollingStock::GetRailConstraintProfile() const
{
	return Profile;
}

void ARollingStock::SetRailConstraintProfile(const FRailConstraintProfile& NewProfile)
{
	Profile = NewProfile;
}

void ARollingStock::GetBogieLocations(TArray<FRailLocation>& OutLocations) const
{
	OutLocations = BogieLocations;
}

void ARollingStock::SetBogieLocations(const TArray<FRailLocation>& InLocations)
{	
	BogieLocations = InLocations;
}

void ARollingStock::UpdateBogieLocationsFromNetwork()
{
}

FRollingStockID ARollingStock::GetRollingStockID() const
{
	return ID;
}

