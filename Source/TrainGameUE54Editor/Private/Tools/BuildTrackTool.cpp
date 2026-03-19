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

UInteractiveTool* UBuildTrackToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
    UBuildTrackTool* NewTool = NewObject<UBuildTrackTool>(SceneState.ToolManager);
    NewTool->SetWorld(SceneState.World);
    return NewTool;
}

void UBuildTrackTool::SetWorld(UWorld* World)
{
    TargetWorld = World;
}

void UBuildTrackTool::Setup()
{
    UInteractiveTool::Setup();

    USingleClickInputBehavior* LeftClickBehavior = NewObject<USingleClickInputBehavior>();
    LeftClickBehavior->Initialize(this);
    AddInputBehavior(LeftClickBehavior);

    ULocalSingleClickInputBehavior* RightClickBehavior = NewObject<ULocalSingleClickInputBehavior>();
    RightClickBehavior->Initialize();
    RightClickBehavior->SetUseRightMouseButton();
    RightClickBehavior->IsHitByClickFunc = [](const FInputDeviceRay&) { return FInputRayHit(0.f); };
    RightClickBehavior->OnClickedFunc = [this](const FInputDeviceRay&)
        {
            if (ToolState == EBuildTrackState::PlacingB)
            {
                PendingA = FPendingPoint();
                SnapNodeA = FRailNodeID();
                SnapNodeB = FRailNodeID();
                ToolState = EBuildTrackState::Hovering;
                UE_LOG(LogTemp, Warning, TEXT("BuildTool: Cancelled"));
            }
        };
    AddInputBehavior(RightClickBehavior);

    UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
    HoverBehavior->Initialize(this);
    AddInputBehavior(HoverBehavior);

    UMouseWheelInputBehavior* ScrollBehavior = NewObject<UMouseWheelInputBehavior>();
    ScrollBehavior->Initialize(this);
    AddInputBehavior(ScrollBehavior);

    Properties = NewObject<UBuildTrackToolProperties>(this);
    AddToolPropertySource(Properties);
}

// ---- Raycast ----

bool UBuildTrackTool::RaycastToWorld(const FInputDeviceRay& Ray, FVector& OutPos, FVector& OutNormal) const
{
    FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
    FHitResult HitResult;
    if (TargetWorld->LineTraceSingleByObjectType(HitResult, Ray.WorldRay.Origin, Ray.WorldRay.PointAt(999999), QueryParams))
    {
        OutPos = HitResult.ImpactPoint;
        OutNormal = HitResult.ImpactNormal;
        return true;
    }

    FPlane GroundPlane(FVector::ZeroVector, FVector::UpVector);
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
        const FVector NodePos = RailNetwork->GetNodeTransform(Pair.Value.ID).GetLocation();
        FVector ToNode = NodePos - Ray.WorldRay.Origin;
        float T = FVector::DotProduct(ToNode, Ray.WorldRay.Direction);
        if (T < 0.f) continue;

        float DistSq = FVector::DistSquared(Ray.WorldRay.Origin + Ray.WorldRay.Direction * T, NodePos);
        if (DistSq < FMath::Square(SnapRadius) && DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            OutNodeID = Pair.Value.ID;
            bFound = true;
        }
    }

    return bFound;
}

bool UBuildTrackTool::RaycastToEdge(const FInputDeviceRay& Ray, FRailEdgeID& OutEdgeID) const
{
    URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return false;

    FRailLocation ClosestLoc;
    float DistSq = TNumericLimits<float>::Max();

    if (!RailNetwork->FindClosestRailLocation(CursorPos, ClosestLoc, DistSq))
        return false;

    const float SnapRadiusSq = FMath::Square(100.f);
    if (DistSq > SnapRadiusSq) return false;

    OutEdgeID = ClosestLoc.Edge;
    return true;
}

FVector UBuildTrackTool::ComputeTangentFromRotation(const FVector& Normal) const
{
    return FQuat(Normal, FMath::DegreesToRadians(TangentRotationDeg))
        .RotateVector(FVector::ForwardVector)
        .GetSafeNormal();
}

FTransform UBuildTrackTool::BuildNodeTransform(const FVector& Position, const FVector& SurfaceNormal, const FVector& TangentDir) const
{
    FVector Forward = TangentDir.GetSafeNormal();
    FVector Right = FVector::CrossProduct(SurfaceNormal.GetSafeNormal(), Forward).GetSafeNormal();
    FVector TrueUp = FVector::CrossProduct(Forward, Right).GetSafeNormal();
    return FTransform(FRotationMatrix::MakeFromXZ(Forward, TrueUp).Rotator(), Position);
}

FEdgePlacementRequest UBuildTrackTool::BuildRequest(
    const FPendingPoint& A, FRailNodeID SnapA,
    const FVector& PosB, const FVector& NormalB, const FVector& TanB, FRailNodeID SnapB) const
{
    FEdgePlacementRequest Request;
    Request.ExistingNodeA = SnapA;
    Request.TransformA = BuildNodeTransform(A.Position, A.Normal, A.Tangent);
    Request.TangentA = A.Tangent;
    Request.ExistingNodeB = SnapB;
    Request.TransformB = BuildNodeTransform(PosB, NormalB, TanB);
    Request.TangentB = TanB;
    return Request;
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
    if (!RailNetwork) return true;

    // Compute base cursor tangent from scroll wheel
    CursorTangent = ComputeTangentFromRotation(CursorNormal);

    FRailNodeID NearestNode;
    if (RaycastToNode(DevicePos, NearestNode))
    {
        CursorPos = RailNetwork->GetNodeTransform(NearestNode).GetLocation();
        bSnapping = true;

        if (ToolState == EBuildTrackState::Hovering)
            SnapNodeA = NearestNode;
        else
            SnapNodeB = NearestNode;

        // Snap tangent to nearest family on the hovered node
        FRailNodeID SnapNode = (ToolState == EBuildTrackState::Hovering) ? SnapNodeA : SnapNodeB;
        if (SnapNode.IsValid())
            CursorTangent = RailNetwork->GetSnappedTangentForNode(SnapNode, CursorTangent);
    }
    else
    {
        bSnapping = false;
        if (ToolState == EBuildTrackState::Hovering)
            SnapNodeA = FRailNodeID();
        else
            SnapNodeB = FRailNodeID();
    }

    if (ToolState == EBuildTrackState::PlacingB)
    {
        FEdgePlacementRequest PreviewRequest = BuildRequest(PendingA, SnapNodeA, CursorPos, CursorNormal, CursorTangent, SnapNodeB);
        bPlacementValid = RailNetwork->CanPlaceEdge(PreviewRequest);
    }

    Properties->GhostPosition = CursorPos;
    Properties->TangentRotationDeg = TangentRotationDeg;
    return true;
}

void UBuildTrackTool::OnEndHover() {}

// ---- Scroll ----

FInputRayHit UBuildTrackTool::ShouldRespondToMouseWheel(const FInputDeviceRay& CurrentPos)
{
    return FSlateApplication::Get().GetModifierKeys().IsAltDown() ? FInputRayHit(0.f) : FInputRayHit();
}

void UBuildTrackTool::OnMouseWheelScrollUp(const FInputDeviceRay& CurrentPos)
{
    TangentRotationDeg += TangentRotationIncrement;
    if (TangentRotationDeg >= 360.f) TangentRotationDeg -= 360.f;
    Properties->TangentRotationDeg = TangentRotationDeg;
}

void UBuildTrackTool::OnMouseWheelScrollDown(const FInputDeviceRay& CurrentPos)
{
    TangentRotationDeg -= TangentRotationIncrement;
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
    if (!RailNetwork) return;

    // Handle removal if shift is held
    bool bShiftHeld = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
    if (ToolState == EBuildTrackState::Hovering && bShiftHeld)
    {
        FRailEdgeID EdgeID;
        RaycastToEdge(ClickPos, EdgeID);
        RailNetwork->RequestRemoveEdge(EdgeID);
        UE_LOG(LogTemp, Warning, TEXT("BuildTool: Edge removed successfully"));
        return;
    }
    if (ToolState == EBuildTrackState::Hovering)
    {
        PendingA.Position = CursorPos;
        PendingA.Normal = CursorNormal;
        PendingA.Tangent = CursorTangent;
        ToolState = EBuildTrackState::PlacingB;

        UE_LOG(LogTemp, Warning, TEXT("BuildTool: Point A set at %s, Tangent %s"),
            *PendingA.Position.ToString(), *PendingA.Tangent.ToString());
    }
    else if (ToolState == EBuildTrackState::PlacingB)
    {
        FVector TangentB = CursorTangent;
        FEdgePlacementRequest Request = BuildRequest(PendingA, SnapNodeA, CursorPos, CursorNormal, TangentB, SnapNodeB);

        if (RailNetwork->RequestPlaceEdge(Request))
        {
            UE_LOG(LogTemp, Warning, TEXT("BuildTool: Edge placed successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BuildTool: Edge placement rejected"));
        }

        PendingA = FPendingPoint();
        SnapNodeA = FRailNodeID();
        SnapNodeB = FRailNodeID();
        bPlacementValid = false;
        ToolState = EBuildTrackState::Hovering;
    }
}

// ---- Render ----

void UBuildTrackTool::Render(IToolsContextRenderAPI* RenderAPI)
{
    if (!TargetWorld) return;

    FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
    if (!PDI) return;

    FLinearColor GhostColor = bSnapping
        ? FLinearColor::Green
        : (bPlacementValid ? FLinearColor::Yellow : FLinearColor::Red);

    if (ToolState != EBuildTrackState::PlacingB)
        GhostColor = bSnapping ? FLinearColor::Green : FLinearColor::White;

    PDI->DrawPoint(CursorPos, GhostColor, 12.f, SDPG_Foreground);

    PDI->DrawLine(CursorPos, CursorPos + CursorTangent * 200.f, FLinearColor::White, SDPG_Foreground, 2.f);

    if (ToolState == EBuildTrackState::PlacingB)
    {
        PDI->DrawPoint(PendingA.Position, FLinearColor::Green, 12.f, SDPG_Foreground);

        float Dist = FVector::Distance(PendingA.Position, CursorPos);
        FLinearColor CurveColor = bPlacementValid ? FLinearColor::Yellow : FLinearColor::Red;

        FVector Prev = PendingA.Position;
        for (int32 i = 1; i <= 32; ++i)
        {
            const float T = (float)i / 32.f;
            const float TT = T * T, TTT = TT * T;
            FVector Curr =
                (2 * TTT - 3 * TT + 1) * PendingA.Position +
                (TTT - 2 * TT + T) * (PendingA.Tangent * Dist) +
                (-2 * TTT + 3 * TT) * CursorPos +
                (TTT - TT) * (CursorTangent * Dist);
            PDI->DrawLine(Prev, Curr, CurveColor, SDPG_Foreground, 2.f);
            Prev = Curr;
        }
    }
}

#undef LOCTEXT_NAMESPACE