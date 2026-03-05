#include "Tools/BuildTrackTool.h"
#include "InteractiveToolManager.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"
#include "Subsystems/RailNetworkSubsystem.h"
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

    // Click behavior
    USingleClickInputBehavior* ClickBehavior = NewObject<USingleClickInputBehavior>();
    ClickBehavior->Initialize(this);
    AddInputBehavior(ClickBehavior);

    // Hover behavior
    UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
    HoverBehavior->Initialize(this);
    AddInputBehavior(HoverBehavior);

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
    UE_LOG(LogTemp, Warning, TEXT("OnUpdateHover called"));
    RaycastToWorld(DevicePos, CursorPos, CursorNormal);

    // TODO: check if cursor is near existing node for snapping
    // SnapNodeA = FindNearestNode(CursorPos, SnapThreshold);

    Properties->GhostPosition = CursorPos;
    Properties->TangentRotationDeg = TangentRotationDeg;
    return true;
}

void UBuildTrackTool::OnEndHover()
{
}

// ---- Click ----

FInputRayHit UBuildTrackTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
    return FInputRayHit(0.f);
}

void UBuildTrackTool::OnClicked(const FInputDeviceRay& ClickPos)
{
    FVector HitPos, HitNormal;
    RaycastToWorld(ClickPos, HitPos, HitNormal);

    if (ToolState == EBuildTrackState::Hovering)
    {
        // Click 1 - store point A
        PointA = HitPos;
        TangentA = ComputeTangentFromRotation(HitNormal);
        SnapNodeA = FRailNodeID(); // clear snap
        ToolState = EBuildTrackState::PlacingB;

        UE_LOG(LogTemp, Warning, TEXT("BuildTool: Point A set at %s, Tangent %s"),
            *PointA.ToString(), *TangentA.ToString());
    }
    else if (ToolState == EBuildTrackState::PlacingB)
    {
        // Click 2 - confirm edge
        FVector PointB = HitPos;
        FVector TangentB = ComputeTangentFromRotation(HitNormal);

        URailNetworkSubsystem* RailNetwork = TargetWorld->GetSubsystem<URailNetworkSubsystem>();
        if (!RailNetwork)
        {
            ToolState = EBuildTrackState::Hovering;
            return;
        }

        // Create nodes
        FRailNodeID NodeA = SnapNodeA.IsValid()
            ? SnapNodeA
            : RailNetwork->CreateNode(PointA, ERailNodeType::Control);

        FRailNodeID NodeB = RailNetwork->CreateNode(PointB, ERailNodeType::Control);

        // Scale tangents by distance
        float Dist = FVector::Distance(PointA, PointB);
        RailNetwork->CreateEdge(NodeA, NodeB, TangentA * Dist, TangentB * Dist);

        UE_LOG(LogTemp, Warning, TEXT("BuildTool: Edge created from node %d to node %d"),
            NodeA.Value, NodeB.Value);

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
    PDI->DrawPoint(CursorPos, FLinearColor::White, 12.f, SDPG_Foreground);

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

#undef LOCTEXT_NAMESPACE