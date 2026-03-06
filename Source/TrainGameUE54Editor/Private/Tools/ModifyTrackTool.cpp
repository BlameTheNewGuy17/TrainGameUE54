#include "Tools/ModifyTrackTool.h"
#include "InteractiveToolManager.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "SceneManagement.h"
#include "ToolContextInterfaces.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseBehaviors/KeyAsModifierInputBehavior.h"
#include "Engine/World.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "ModifyTrackTool.h"

#define LOCTEXT_NAMESPACE "UModifyTrackTool"

// ---- Builder ----

UInteractiveTool* UModifyTrackToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
    UModifyTrackTool* NewTool = NewObject<UModifyTrackTool>(SceneState.ToolManager);
    NewTool->SetWorld(SceneState.World);
    return NewTool;
}

// ---- Tool ----

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

    RebuildMirror();
}

void UModifyTrackTool::Shutdown(EToolShutdownType ShutdownType)
{
    DeselectNode();
    UInteractiveTool::Shutdown(ShutdownType);
}

void UModifyTrackTool::OnTick(float DeltaTime)
{
    if (TransformGizmo && TransformProxy && bHasSelection)
    {
        // Check if gizmo moved the transform
        FTransform CurrentGizmoTransform = TransformProxy->GetTransform();

        URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
        if (!RailNetwork) return;

        FRailNodeData NodeData;
        if (RailNetwork->GetNodeData(SelectedNodeID, NodeData))
        {
            if (!CurrentGizmoTransform.Equals(NodeData.Transform, 0.1f))
            {
                RailNetwork->SetNodeTransform(SelectedNodeID, CurrentGizmoTransform);
                RailNetwork->OnNodeTransformChanged(SelectedNodeID);
                UpdatePropertiesFromNode();

                RebuildMirror();
            }
        }
    }
}

void UModifyTrackTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
    if (!bHasSelection) return;

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FRailNodeData NodeData;
    if (!RailNetwork->GetNodeData(SelectedNodeID, NodeData)) return;

    // Apply property panel changes back to node
    NodeData.Transform.SetLocation(Properties->Position);
    NodeData.Transform.SetRotation(Properties->Orientation.Quaternion());
    NodeData.Type = Properties->NodeType;

    RailNetwork->SetNodeTransform(SelectedNodeID, NodeData.Transform);
    RailNetwork->SetNodeType(SelectedNodeID, Properties->NodeType);
    RailNetwork->OnNodeTransformChanged(SelectedNodeID);


    RebuildMirror();

    // Update gizmo position to match
    if (TransformProxy)
    {
        TransformProxy->SetTransform(NodeData.Transform);
    }
}

// ---- Selection ----

void UModifyTrackTool::SelectNode(FRailNodeID NodeID)
{
    UE_LOG(LogTemp, Warning, TEXT("SelectNode called with ID=%d"), NodeID.Value);

    DeselectNode();

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FRailNodeData NodeData;
    if (!RailNetwork->GetNodeData(NodeID, NodeData)) return;

    SelectedNodeID = NodeID;
    bHasSelection = true;

    // Create transform proxy with custom get/set
    TransformProxy = NewObject<UTransformProxy>(this);
    TransformProxy->OnTransformChanged.AddLambda(
        [this](UTransformProxy*, FTransform NewTransform)
        {
            URailNetworkSubsystem* RN = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
            if (!RN) return;
            RN->SetNodeTransform(SelectedNodeID, NewTransform);
            RN->OnNodeTransformChanged(SelectedNodeID);
            UpdatePropertiesFromNode();
            RebuildMirror();
        }
    );
    TransformProxy->SetTransform(NodeData.Transform);

    // Create gizmo
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

    // Clear properties panel
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

    Properties->NodeID = SelectedNodeID.Value;
    Properties->Position = NodeData.Transform.GetLocation();
    Properties->Orientation = NodeData.Transform.GetRotation().Rotator();
    Properties->NodeType = NodeData.Type;
    Properties->ConnectedEdgeCount = NodeData.ConnectedEdges.Num();
}

// ---- Hover ----

FInputRayHit UModifyTrackTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
    return FInputRayHit(0.f);
}

void UModifyTrackTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
}

bool UModifyTrackTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
    FVector HitPos;
    if (!RaycastToWorld(DevicePos, HitPos)) return true;

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return true;

    FRailNodeID Nearest = FRailNodeID();

    RaycastToNode(DevicePos, Nearest);
    
    if (Nearest.IsValid())
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
    FVector HitPos;
    RaycastToWorld(ClickPos, HitPos);

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    FRailNodeID Nearest = FRailNodeID();
    RaycastToNode(ClickPos, Nearest);
    UE_LOG(LogTemp, Warning, TEXT("OnClicked nearest valid=%d"), Nearest.IsValid());
    if (Nearest.IsValid())
    {
        SelectNode(Nearest);
    }
    else
    {
        DeselectNode();
    }
}

// ---- Raycast ----

bool UModifyTrackTool::RaycastToWorld(const FInputDeviceRay& Ray, FVector& OutPos) const
{
    FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
    FHitResult HitResult;
    bool bHit = TargetWorld->LineTraceSingleByObjectType(
        HitResult,
        Ray.WorldRay.Origin,
        Ray.WorldRay.PointAt(999999),
        QueryParams);

    if (bHit)
    {
        OutPos = HitResult.ImpactPoint;
        return true;
    }

    FPlane GroundPlane(FVector::ZeroVector, FVector::UpVector);
    OutPos = FMath::RayPlaneIntersection(Ray.WorldRay.Origin, Ray.WorldRay.Direction, GroundPlane);
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
        State.Position = Pair.Value.Transform.GetLocation();
        State.State = ENodeDisplayState::Default;
        NodeMirror.Add(State);
    }
}

bool UModifyTrackTool::RaycastToNode(const FInputDeviceRay& Ray, FRailNodeID& OutNodeID) const
{
    UE_LOG(LogTemp, Warning, TEXT("RaycastToNode checking %d nodes"), NodeMirror.Num());

    float BestDistSq = TNumericLimits<float>::Max();
    bool bFound = false;

    for (const FNodeRenderState& Node : NodeMirror)
    {
        // Project node position onto ray
        FVector ToNode = Node.Position - Ray.WorldRay.Origin;
        float T = FVector::DotProduct(ToNode, Ray.WorldRay.Direction);
        if (T < 0.f) continue;

        FVector Closest = Ray.WorldRay.Origin + Ray.WorldRay.Direction * T;
        float DistSq = FVector::DistSquared(Closest, Node.Position);
        UE_LOG(LogTemp, Warning, TEXT("  Node %d at %s T=%.1f DistSq=%.1f Radius=%.1f"),
            Node.ID.Value, *Node.Position.ToString(), T, DistSq, Node.Radius);
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