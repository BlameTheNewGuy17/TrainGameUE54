#include "TrainNetworkPlayerComponent.h"
#include "TrainNetworkStateComponent.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"


UTrainNetworkPlayerComponent::UTrainNetworkPlayerComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

URailNetworkSubsystem* UTrainNetworkPlayerComponent::GetRailNetwork() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<URailNetworkSubsystem>() : nullptr;
}

UTrainNetworkStateComponent* UTrainNetworkPlayerComponent::GetStateComponent() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    AGameStateBase* GameState = World->GetGameState();
    if (!GameState) return nullptr;

    return GameState->FindComponentByClass<UTrainNetworkStateComponent>();
}

void UTrainNetworkPlayerComponent::Server_RequestPlaceEdge_Implementation(const FEdgePlacementRequest& Request)
{
    URailNetworkSubsystem* RailNetwork = GetRailNetwork();
    if (!RailNetwork) return;

    FRailEdgeData OutEdge;
    FRailNodeData OutNodeA, OutNodeB;

    if (!RailNetwork->RequestPlaceEdge(Request, OutEdge, OutNodeA, OutNodeB)) return;

    UTrainNetworkStateComponent* StateComp = GetStateComponent();
    if (!StateComp) return;

    FRailNodeSnapshot SnapA = FromNodeData(OutNodeA);
    FRailNodeSnapshot SnapB = FromNodeData(OutNodeB);

    StateComp->Multicast_OnEdgePlaced(OutEdge, SnapA, SnapB);
}

void UTrainNetworkPlayerComponent::Server_RequestRemoveEdge_Implementation(FRailEdgeID EdgeID)
{
    URailNetworkSubsystem* RailNetwork = GetRailNetwork();
    if (!RailNetwork) return;

    if (!RailNetwork->CanRemoveEdge(EdgeID)) return;
    if (!RailNetwork->RequestRemoveEdge(EdgeID)) return;

    UTrainNetworkStateComponent* StateComp = GetStateComponent();
    if (!StateComp) return;

    StateComp->Multicast_OnEdgeRemoved(EdgeID);
}