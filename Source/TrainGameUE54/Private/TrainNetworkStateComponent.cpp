#include "TrainNetworkStateComponent.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "RailNetworkTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"


UTrainNetworkStateComponent::UTrainNetworkStateComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UTrainNetworkStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UTrainNetworkStateComponent::Multicast_OnEdgePlaced_Implementation(const FRailEdgeData& Edge, const FRailNodeSnapshot& NodeA, const FRailNodeSnapshot& NodeB)
{
    // Server already applied this — skip on server
    if (GetOwner()->HasAuthority()) return;

    URailNetworkSubsystem* RailNetwork = GetRailNetwork();
    if (!RailNetwork) return;

    RailNetwork->ApplyRemoteEdgePlacement(Edge, NodeA, NodeB);
}

void UTrainNetworkStateComponent::Client_ReceiveFullSnapshot_Implementation(APlayerController* PC, const TArray<FRailNodeSnapshot>& Nodes, const TArray<FRailEdgeData>& Edges)
{
    URailNetworkSubsystem* RailNetwork = GetRailNetwork();
    if (!RailNetwork) return;

    RailNetwork->LoadSnapshot(Nodes, Edges);
}

void UTrainNetworkStateComponent::Multicast_OnEdgeRemoved_Implementation(FRailEdgeID EdgeID)
{
    if (GetOwner()->HasAuthority()) return;

    URailNetworkSubsystem* RailNetwork = GetRailNetwork();
    if (!RailNetwork) return;

    RailNetwork->RequestRemoveEdge(EdgeID);
}

void UTrainNetworkStateComponent::PushSnapshotToClient(APlayerController* PC)
{
    if (!GetOwner()->HasAuthority()) return;

    URailNetworkSubsystem* RailNetwork = GetRailNetwork();
    if (!RailNetwork) return;

    TArray<FRailNodeSnapshot> Nodes;
    TArray<FRailEdgeData> Edges;

    for (const auto& Pair : RailNetwork->GetNodes())
    {
        FRailNodeSnapshot TempSnapshot;
        TempSnapshot.ID = Pair.Value.ID;
        TempSnapshot.WorldTransform = Pair.Value.WorldTransform;
        TempSnapshot.Type = Pair.Value.Type;
        TempSnapshot.ConnectedEdges = Pair.Value.ConnectedEdges;
        Nodes.Add(TempSnapshot);
    }
    for (const auto& Pair : RailNetwork->GetEdges())
        Edges.Add(Pair.Value);

    Client_ReceiveFullSnapshot(PC, Nodes, Edges);
}

URailNetworkSubsystem* UTrainNetworkStateComponent::GetRailNetwork() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<URailNetworkSubsystem>() : nullptr;
}