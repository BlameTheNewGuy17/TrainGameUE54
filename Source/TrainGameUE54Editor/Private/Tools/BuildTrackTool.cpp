#include "Tools/BuildTrackTool.h"
#include "InteractiveToolManager.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "BaseBehaviors/MouseWheelBehavior.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"

#define LOCTEXT_NAMESPACE "UBuildTrackTool"

// ---- Builder ----

UInteractiveTool* UBuildTrackToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
    UBuildTrackTool* NewTool = NewObject<UBuildTrackTool>(SceneState.ToolManager);
    NewTool->SetWorld(SceneState.World);
    return NewTool;
}

// ---- Tool ----

void UBuildTrackTool::SetWorld(UWorld* World)
{
    TargetWorld = World;
}

void UBuildTrackTool::Setup()
{
    UInteractiveTool::Setup();

    // Left Click behavior
    USingleClickInputBehavior* LeftClickBehavior = NewObject<USingleClickInputBehavior>();
    LeftClickBehavior->Initialize(this);
    AddInputBehavior(LeftClickBehavior);

    // Right Click behavior
    ULocalSingleClickInputBehavior* RightClickBehavior = NewObject<ULocalSingleClickInputBehavior>();
    RightClickBehavior->Initialize();
    RightClickBehavior->SetUseRightMouseButton();
    RightClickBehavior->IsHitByClickFunc = [](const FInputDeviceRay& ClickPos) {
        return FInputRayHit(0.f);
        };
    RightClickBehavior->OnClickedFunc = [this](const FInputDeviceRay& ClickPos) {
        if (ToolState == EBuildTrackState::PlacingB)
        {
            SnapNodeA = FRailNodeID();
            SnapNodeB = FRailNodeID();
            ToolState = EBuildTrackState::Hovering;
            UE_LOG(LogTemp, Warning, TEXT("BuildTool: Cancelled"));
        }
    };
    AddInputBehavior(RightClickBehavior);
    
    // Hover behavior
    UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
    HoverBehavior->Initialize(this);
    AddInputBehavior(HoverBehavior);

    // Mouse Wheel behavior
    UMouseWheelInputBehavior* ScrollBehavior = NewObject<UMouseWheelInputBehavior>();
    ScrollBehavior->Initialize(this);
    AddInputBehavior(ScrollBehavior);

    Properties = NewObject<UBuildTrackToolProperties>(this);

    AddToolPropertySource(Properties);
}

bool UBuildTrackTool::RaycastToWorld(const FInputDeviceRay& Ray, FVector& OutPos, FVector& OutNormal) const
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
        OutNormal = HitResult.ImpactNormal;
        return true;
    }

    // Fall back to ground plane
    FPlane GroundPlane(FVector(0, 0, 0), FVector(0, 0, 1));
    OutPos = FMath::RayPlaneIntersection(Ray.WorldRay.Origin, Ray.WorldRay.Direction, GroundPlane);
    OutNormal = FVector::UpVector;
    return false;
}

bool UBuildTrackTool::RaycastToNode(const FInputDeviceRay& Ray, FRailNodeID& OutNodeID) const
{
    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return false;

    const float SnapRadius = 50.f;
    float BestDistSq = TNumericLimits<float>::Max();
    bool bFound = false;

    for (const auto& Pair : RailNetwork->GetNodes())
    {
        const FVector NodePos = RailNetwork->GetNodeTransform(FRailNodeID{ Pair.Key }).GetLocation();
        FVector ToNode = NodePos - Ray.WorldRay.Origin;
        float T = FVector::DotProduct(ToNode, Ray.WorldRay.Direction);
        if (T < 0.f) continue;

        FVector Closest = Ray.WorldRay.Origin + Ray.WorldRay.Direction * T;
        float DistSq = FVector::DistSquared(Closest, NodePos);

        if (DistSq < FMath::Square(SnapRadius) && DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            OutNodeID = Pair.Value.ID;
            bFound = true;
        }
    }

    return bFound;
}

FVector UBuildTrackTool::ComputeTangentFromRotation(const FVector& Normal) const
{
    // Start with world forward, rotate around the surface normal by TangentRotationDeg
    FVector Base = FVector::ForwardVector;
    FQuat Rot = FQuat(Normal, FMath::DegreesToRadians(TangentRotationDeg));
    return Rot.RotateVector(Base).GetSafeNormal();
}

// ---- Hover ----

FInputRayHit UBuildTrackTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
    return FInputRayHit(0.f);
}

void UBuildTrackTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
    RaycastToWorld(DevicePos, CursorPos, CursorNormal);
}

bool UBuildTrackTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
    RaycastToWorld(DevicePos, CursorPos, CursorNormal);

    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (RailNetwork)
    {
        FRailNodeID NearestNode = FRailNodeID();
        RaycastToNode(DevicePos, NearestNode);
        if (NearestNode.IsValid())
        {
            FRailNodeData NodeData;
            RailNetwork->GetNodeData(NearestNode, NodeData);
            CursorPos = RailNetwork->GetNodeTransform(NodeData.ID).GetLocation();
            bSnapping = true;

            if (ToolState == EBuildTrackState::Hovering)
                SnapNodeA = NearestNode;
            else
                SnapNodeB = NearestNode;
        }
        else
        {
            bSnapping = false;
            if (ToolState == EBuildTrackState::Hovering)
                SnapNodeA = FRailNodeID();
            else
                SnapNodeB = FRailNodeID();
        }
    }

    Properties->GhostPosition = CursorPos;
    Properties->TangentRotationDeg = TangentRotationDeg;
    return true;
}

void UBuildTrackTool::OnEndHover()
{
}


// ---- Scroll ----

FInputRayHit UBuildTrackTool::ShouldRespondToMouseWheel(const FInputDeviceRay& CurrentPos)
{
    
    bool bAltHeld = FSlateApplication::Get().GetModifierKeys().IsAltDown();

    return bAltHeld ? FInputRayHit(0.f) : FInputRayHit();
}

void UBuildTrackTool::OnMouseWheelScrollUp(const FInputDeviceRay& CurrentPos)
{
    TangentRotationDeg += 15.f;
    if (TangentRotationDeg >= 360.f) TangentRotationDeg -= 360.f;
    Properties->TangentRotationDeg = TangentRotationDeg;
}

void UBuildTrackTool::OnMouseWheelScrollDown(const FInputDeviceRay& CurrentPos)
{
    TangentRotationDeg -= 15.f;
    if (TangentRotationDeg < 0.f) TangentRotationDeg += 360.f;
    Properties->TangentRotationDeg = TangentRotationDeg;
}


// ---- Click ----

FInputRayHit UBuildTrackTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
    return FInputRayHit(0.f);
}

void UBuildTrackTool::OnClicked(const FInputDeviceRay& ClickPos)
{
    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork)
    {
        ToolState = EBuildTrackState::Hovering;
        return;
    }
    if (ToolState == EBuildTrackState::Hovering)
    {
        // Click 1 - store point A
        PointA = CursorPos;
        NormalA = CursorNormal;
        if (bSnapping && SnapNodeA.IsValid())
        {
            TangentA = RailNetwork->GetContinuationTangent(SnapNodeA, ComputeTangentFromRotation(CursorNormal));
        }
        else
        {
            TangentA = ComputeTangentFromRotation(CursorNormal);
        }
        ToolState = EBuildTrackState::PlacingB;

        UE_LOG(LogTemp, Warning, TEXT("BuildTool: Point A set at %s, Tangent %s"),
            *PointA.ToString(), *TangentA.ToString());
    }
    else if (ToolState == EBuildTrackState::PlacingB)
    {
        // Click 2 - store point B
        FVector PointB = CursorPos;
        FVector NormalB = CursorNormal;
        FVector TangentB = bSnapping && SnapNodeB.IsValid()
            ? RailNetwork->GetContinuationTangent(SnapNodeB, ComputeTangentFromRotation(CursorNormal))
            : ComputeTangentFromRotation(CursorNormal);
        
        FTransform TransformA = BuildNodeTransform(PointA, NormalA, TangentA);
        FRailNodeID NodeA = SnapNodeA.IsValid()
            ? SnapNodeA
            : RailNetwork->CreateNode(TransformA, ERailNodeType::Control);

        FTransform TransformB = BuildNodeTransform(PointB, NormalB, TangentB);
        FRailNodeID NodeB = SnapNodeB.IsValid()
            ? SnapNodeB
            : RailNetwork->CreateNode(TransformB, ERailNodeType::Control);

        float Dist = FVector::Distance(PointA, PointB);
        FVector ScaledTangentA = TangentA * Dist;
        FVector ScaledTangentB = TangentB * Dist;
        RailNetwork->CreateEdge(NodeA, NodeB, &ScaledTangentA, &ScaledTangentB);

        FRailNodeData DataA, DataB;
        RailNetwork->GetNodeData(NodeA, DataA);
        RailNetwork->GetNodeData(NodeB, DataB);
        UE_LOG(LogTemp, Warning, TEXT("NodeA pos: %s"), *RailNetwork->GetNodeTransform(DataA.ID).GetLocation().ToString());
        UE_LOG(LogTemp, Warning, TEXT("NodeB pos: %s"), *RailNetwork->GetNodeTransform(DataB.ID).GetLocation().ToString());

        UE_LOG(LogTemp, Warning, TEXT("BuildTool: Edge created from node %d to node %d"),
            NodeA.Value, NodeB.Value);

        // Reset state
        SnapNodeA = FRailNodeID();
        SnapNodeB = FRailNodeID();
        ToolState = EBuildTrackState::Hovering;
    }
}

// ---- Render ----

void UBuildTrackTool::Render(IToolsContextRenderAPI* RenderAPI)
{
    if (!TargetWorld) return;

    FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
    if (!PDI) return;

    // Draw ghost node at cursor
    FLinearColor GhostColor = bSnapping ? FLinearColor::Green : FLinearColor::White;
    PDI->DrawPoint(CursorPos, GhostColor, 12.f, SDPG_Foreground);

    // Draw tangent direction indicator
    FVector Tangent = ComputeTangentFromRotation(CursorNormal);
    PDI->DrawLine(CursorPos, CursorPos + Tangent * 200.f, FLinearColor::White, SDPG_Foreground, 2.f);

    // If in PlacingB state, draw preview curve from A to cursor
    if (ToolState == EBuildTrackState::PlacingB)
    {
        // Draw point A
        PDI->DrawPoint(PointA, FLinearColor::Green, 12.f, SDPG_Foreground);

        // Draw preview curve
        float Dist = FVector::Distance(PointA, CursorPos);
        FVector TangentB = ComputeTangentFromRotation(CursorNormal);

        FVector Prev = PointA;
        for (int32 i = 1; i <= 32; ++i)
        {
            const float T = (float)i / 32.f;
            const float TT = T * T;
            const float TTT = TT * T;

            const float H00 = 2 * TTT - 3 * TT + 1;
            const float H10 = TTT - 2 * TT + T;
            const float H01 = -2 * TTT + 3 * TT;
            const float H11 = TTT - TT;

            FVector Curr = H00 * PointA + H10 * (TangentA * Dist) + H01 * CursorPos + H11 * (TangentB * Dist);
            PDI->DrawLine(Prev, Curr, FLinearColor::Yellow, SDPG_Foreground, 2.f);
            Prev = Curr;
        }
    }
}

// ---- MathHelper ----

FTransform UBuildTrackTool::BuildNodeTransform(const FVector& Position, const FVector& SurfaceNormal, const FVector& TangentDir)
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