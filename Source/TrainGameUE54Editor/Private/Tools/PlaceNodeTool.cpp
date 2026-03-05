#include "PlaceNodeTool.h"
#include "InteractiveToolManager.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "ToolContextInterfaces.h"
#include "Subsystems/RailNetworkSubsystem.h"

#define LOCTEXT_NAMESPACE "UPlaceNodeTool"

UInteractiveTool* UPlaceNodeToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
    UPlaceNodeTool* NewTool = NewObject<UPlaceNodeTool>(SceneState.ToolManager);
    NewTool->SetWorld(SceneState.World);
    return NewTool;
}

void UPlaceNodeTool::SetWorld(UWorld* World)
{
    TargetWorld = World;
}

void UPlaceNodeTool::Setup()
{
    USingleClickTool::Setup();
    Properties = NewObject<UPlaceNodeToolProperties>(this);
    AddToolPropertySource(Properties);
}

void UPlaceNodeTool::OnClicked(const FInputDeviceRay& ClickPos)
{
    if (!TargetWorld) return;

    // Raycast into world to find click position
    FVector RayStart = ClickPos.WorldRay.Origin;
    FVector RayEnd = ClickPos.WorldRay.PointAt(999999);
    FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
    FHitResult HitResult;

    FVector NodePos;
    bool bHit = TargetWorld->LineTraceSingleByObjectType(HitResult, RayStart, RayEnd, QueryParams);
    if (bHit)
    {
        NodePos = HitResult.ImpactPoint + FVector(0, 0, Properties->SnapToGroundOffset);
    }
    else
    {
        // Fall back to ground plane at Z=0
        FPlane GroundPlane(FVector(0, 0, 0), FVector(0, 0, 1));
        NodePos = FMath::RayPlaneIntersection(ClickPos.WorldRay.Origin, ClickPos.WorldRay.Direction, GroundPlane);
    }

    // Place node in RailNetworkSubsystem
    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FRailNodeID NewNodeID = RailNetwork->CreateNode(BuildNodeTransform(NodePos, FVector(), FVector()), Properties->NodeType);
    UE_LOG(LogTemp, Warning, TEXT("Placed rail node %d at %s"), NewNodeID.Value, *NodePos.ToString());
}

FTransform UPlaceNodeTool::BuildNodeTransform(const FVector& Position, const FVector& SurfaceNormal, const FVector& TangentDir)
{
    FVector Forward = TangentDir.GetSafeNormal();
    FVector Up = SurfaceNormal.GetSafeNormal();
    // Orthonormalize
    FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
    FVector TrueUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();
    FMatrix RotMat = FRotationMatrix::MakeFromXZ(Forward, TrueUp);
    return FTransform(RotMat.Rotator(), Position);
}

#undef LOCTEXT_NAMESPACE