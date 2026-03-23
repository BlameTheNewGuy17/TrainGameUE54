#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RailNetworkTypes.h"
#include "TrainNetworkPlayerComponent.generated.h"

class URailNetworkSubsystem;
class UTrainNetworkStateComponent;

UCLASS(ClassGroup = (TrainGame), meta = (BlueprintSpawnableComponent))
class TRAINGAMEUE54_API UTrainNetworkPlayerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTrainNetworkPlayerComponent();

    // Client calls these to request network modifications
    UFUNCTION(Server, Reliable) void Server_RequestPlaceEdge(const FEdgePlacementRequest& Request);
    UFUNCTION(Server, Reliable) void Server_RequestRemoveEdge(FRailEdgeID EdgeID);

private:
    URailNetworkSubsystem* GetRailNetwork() const;
    UTrainNetworkStateComponent* GetStateComponent() const;
};