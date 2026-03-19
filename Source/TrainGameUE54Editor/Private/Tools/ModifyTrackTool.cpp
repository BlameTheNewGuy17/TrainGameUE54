#include "Tools/ModifyTrackTool.h"
#include "InteractiveToolManager.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "SceneManagement.h"
#include "ToolContextInterfaces.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "Subsystems/RailNetworkSubsystem.h"

#define LOCTEXT_NAMESPACE "UModifyTrackTool"

UInteractiveTool* UModifyTrackToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
    UModifyTrackTool* NewTool = NewObject<UModifyTrackTool>(SceneState.ToolManager);
    NewTool->SetWorld(SceneState.World);
    return NewTool;
}

void UModifyTrackTool::SetWorld(UWorld* World)
{
    TargetWorld = World;
}

void UModifyTrackTool::Setup()
{
    UInteractiveTool::Setup();

    USingleClickInputBehavior* ClickBehavior = NewObject<USingleClickInputBehavior>();
    ClickBehavior->Initialize(this);
    AddInputBehavior(ClickBehavior);

    UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
    HoverBehavior->Initialize(this);
    AddInputBehavior(HoverBehavior);

    Properties = NewObject<UModifyTrackToolProperties>(this);
    AddToolPropertySource(Properties);


    // Debug print network data before mirror rebuild
    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;
    RailNetwork->PrintNetworkData();

    
    RebuildMirror();
}

void UModifyTrackTool::Shutdown(EToolShutdownType ShutdownType)
{
    DeselectNode();
    UInteractiveTool::Shutdown(ShutdownType);
}

void UModifyTrackTool::OnTick(float DeltaTime)
{
    if (!TransformGizmo || !TransformProxy || !bHasSelection) return;

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FTransform CurrentGizmoTransform = TransformProxy->GetTransform();
    FTransform CurrentNodeTransform = RailNetwork->GetNodeTransform(SelectedNodeID);

    if (!CurrentGizmoTransform.Equals(CurrentNodeTransform, 0.1f))
    {
        // Show validity feedback but don't commit yet — commit happens via RequestMoveNode
        bMoveValid = RailNetwork->CanMoveNode(SelectedNodeID, CurrentGizmoTransform);
    }
}

void UModifyTrackTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
    if (!bHasSelection) return;

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FTransform CurrentTransform = RailNetwork->GetNodeTransform(SelectedNodeID);
    CurrentTransform.SetLocation(Properties->Position);
    CurrentTransform.SetRotation(Properties->Orientation.Quaternion());

    if (RailNetwork->RequestMoveNode(SelectedNodeID, CurrentTransform))
    {
        if (TransformProxy)
            TransformProxy->SetTransform(CurrentTransform);
        RebuildMirror();
    }
    else
    {
        // Revert properties panel to actual node state
        UpdatePropertiesFromNode();
    }
}

// ---- Selection ----

void UModifyTrackTool::SelectNode(FRailNodeID NodeID)
{
    DeselectNode();

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FRailNodeData NodeData;
    if (!RailNetwork->GetNodeData(NodeID, NodeData)) return;

    SelectedNodeID = NodeID;
    bHasSelection = true;

    TransformProxy = NewObject<UTransformProxy>(this);
    TransformProxy->OnTransformChanged.AddLambda(
        [this](UTransformProxy*, FTransform NewTransform)
        {
            URailNetworkSubsystem* RN = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
            if (!RN) return;

            if (RN->RequestMoveNode(SelectedNodeID, NewTransform))
            {
                UpdatePropertiesFromNode();
                RebuildMirror();
            }
            else
            {
                // Revert gizmo to last valid position
                TransformProxy->SetTransform(RN->GetNodeTransform(SelectedNodeID));
            }
        }
    );
    TransformProxy->SetTransform(RailNetwork->GetNodeTransform(NodeID));

    TransformGizmo = UE::TransformGizmoUtil::CreateCustomTransformGizmo(
        GetToolManager()->GetPairedGizmoManager(),
        ETransformGizmoSubElements::TranslateAllAxes |
        ETransformGizmoSubElements::TranslateAllPlanes |
        ETransformGizmoSubElements::RotateAllAxes,
        this
    );

    if (!TransformGizmo)
    {
        UE_LOG(LogTemp, Error, TEXT("ModifyTool: Failed to create TransformGizmo"));
        return;
    }

    TransformGizmo->bUseContextGizmoMode = false;
    TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Combined;
    TransformGizmo->SetActiveTarget(TransformProxy);
    TransformGizmo->SetVisibility(true);

    UpdatePropertiesFromNode();
}

void UModifyTrackTool::DeselectNode()
{
    if (TransformGizmo)
    {
        GetToolManager()->GetPairedGizmoManager()->DestroyGizmo(TransformGizmo);
        TransformGizmo = nullptr;
    }

    TransformProxy = nullptr;
    SelectedNodeID = FRailNodeID();
    bHasSelection = false;
    bMoveValid = true;

    Properties->NodeID = -1;
    Properties->Position = FVector::ZeroVector;
    Properties->Orientation = FRotator::ZeroRotator;
    Properties->ConnectedEdgeCount = 0;
}

void UModifyTrackTool::UpdatePropertiesFromNode()
{
    if (!bHasSelection) return;

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FRailNodeData NodeData;
    if (!RailNetwork->GetNodeData(SelectedNodeID, NodeData)) return;

    FTransform NodeTransform = RailNetwork->GetNodeTransform(SelectedNodeID);

    Properties->NodeID = SelectedNodeID.Value;
    Properties->Position = NodeTransform.GetLocation();
    Properties->Orientation = NodeTransform.GetRotation().Rotator();
    Properties->NodeType = NodeData.Type;
    Properties->ConnectedEdgeCount = NodeData.ConnectedEdges.Num();
}

// ---- Hover ----

FInputRayHit UModifyTrackTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
    return FInputRayHit(0.f);
}

void UModifyTrackTool::OnBeginHover(const FInputDeviceRay& DevicePos) {}

bool UModifyTrackTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
    FRailNodeID Nearest;
    if (RaycastToNode(DevicePos, Nearest))
    {
        HoveredNodeID = Nearest;
        bHasHover = true;
    }
    else
    {
        HoveredNodeID = FRailNodeID();
        bHasHover = false;
    }
    return true;
}

void UModifyTrackTool::OnEndHover()
{
    HoveredNodeID = FRailNodeID();
    bHasHover = false;
}

// ---- Click ----

FInputRayHit UModifyTrackTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
    return FInputRayHit(0.f);
}

void UModifyTrackTool::OnClicked(const FInputDeviceRay& ClickPos)
{
    FRailNodeID Nearest;
    if (RaycastToNode(ClickPos, Nearest))
        SelectNode(Nearest);
    else
        DeselectNode();
}

// ---- Raycast ----

bool UModifyTrackTool::RaycastToWorld(const FInputDeviceRay& Ray, FVector& OutPos) const
{
    FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
    FHitResult HitResult;
    if (TargetWorld->LineTraceSingleByObjectType(HitResult, Ray.WorldRay.Origin, Ray.WorldRay.PointAt(999999), QueryParams))
    {
        OutPos = HitResult.ImpactPoint;
        return true;
    }

    OutPos = FMath::RayPlaneIntersection(Ray.WorldRay.Origin, Ray.WorldRay.Direction, FPlane(FVector::ZeroVector, FVector::UpVector));
    return false;
}

void UModifyTrackTool::RebuildMirror()
{
    NodeMirror.Reset();

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    for (const auto& Pair : RailNetwork->GetNodes())
    {
        FNodeRenderState State;
        State.ID = Pair.Value.ID;
        State.Position = RailNetwork->GetNodeTransform(Pair.Value.ID).GetLocation();
        State.Radius = 50.f;
        State.State = ENodeDisplayState::Default;
        NodeMirror.Add(State);
    }
}

bool UModifyTrackTool::RaycastToNode(const FInputDeviceRay& Ray, FRailNodeID& OutNodeID) const
{
    float BestDistSq = TNumericLimits<float>::Max();
    bool bFound = false;

    for (const FNodeRenderState& Node : NodeMirror)
    {
        FVector ToNode = Node.Position - Ray.WorldRay.Origin;
        float T = FVector::DotProduct(ToNode, Ray.WorldRay.Direction);
        if (T < 0.f) continue;

        float DistSq = FVector::DistSquared(Ray.WorldRay.Origin + Ray.WorldRay.Direction * T, Node.Position);
        if (DistSq < FMath::Square(Node.Radius) && DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            OutNodeID = Node.ID;
            bFound = true;
        }
    }

    return bFound;
}

#undef LOCTEXT_NAMESPACE