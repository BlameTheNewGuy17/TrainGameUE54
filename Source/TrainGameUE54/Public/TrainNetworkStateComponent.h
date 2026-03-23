#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RailNetworkTypes.h"
#include "TrainNetworkStateComponent.generated.h"

class APlayerController;
class URailNetworkSubsystem;

UCLASS(ClassGroup = (TrainGame), meta = (BlueprintSpawnableComponent))
class TRAINGAMEUE54_API UTrainNetworkStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTrainNetworkStateComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Called by server to push full snapshot to a specific joining client
    UFUNCTION(Client, Reliable)
    void Client_ReceiveFullSnapshot(APlayerController* PC, const TArray<FRailNodeSnapshot>& Nodes, const TArray<FRailEdgeData>& Edges);

    // Called by server to notify all clients of a placed edge
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnEdgePlaced(const FRailEdgeData& Edge, const FRailNodeSnapshot& NodeA, const FRailNodeSnapshot& NodeB);

    // Called by server to notify all clients of a removed edge
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnEdgeRemoved(FRailEdgeID EdgeID);

    // Called by server when a player joins
    void PushSnapshotToClient(APlayerController* PC);

private:
    URailNetworkSubsystem* GetRailNetwork() const;
};