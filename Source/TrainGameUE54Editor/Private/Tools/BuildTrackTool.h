#pragma once
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/MouseWheelBehavior.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "RailNetworkTypes.h"
#include "BuildTrackTool.generated.h"

UCLASS()
class UBuildTrackToolBuilder : public UInteractiveToolBuilder
{
    GENERATED_BODY()
public:
    virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
    virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UENUM()
enum class EBuildTrackState : uint8
{
    Hovering,   // No clicks yet, ghost node follows mouse
    PlacingB,   // Click 1 done, previewing edge to cursor
};

UCLASS(Transient)
class UBuildTrackToolProperties : public UInteractiveToolPropertySet
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, Category = Status)
    FVector GhostPosition = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = Status)
    float TangentRotationDeg = 0.f;
};

UCLASS()
class UBuildTrackTool : public UInteractiveTool, public IClickBehaviorTarget, public IHoverBehaviorTarget, public IMouseWheelBehaviorTarget
{
    GENERATED_BODY()
public:
    virtual void SetWorld(UWorld* World);
    virtual void Setup() override;
    virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

    // IClickBehaviorTarget
    virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
    virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

    // IHoverBehaviorTarget
    virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
    virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
    virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
    virtual void OnEndHover() override;

    // IMouseWheelBehaviorTarget
    virtual FInputRayHit ShouldRespondToMouseWheel(const FInputDeviceRay& CurrentPos) override;
    virtual void OnMouseWheelScrollUp(const FInputDeviceRay& CurrentPos) override;
    virtual void OnMouseWheelScrollDown(const FInputDeviceRay& CurrentPos) override;



protected:
    UPROPERTY()
    TObjectPtr<UBuildTrackToolProperties> Properties;

    UWorld* TargetWorld = nullptr;

    EBuildTrackState ToolState = EBuildTrackState::Hovering;

    // Point A (first click)
    FVector PointA = FVector::ZeroVector;
    FVector NormalA = FVector::ZeroVector;
    FVector TangentA = FVector::ForwardVector;
    FRailNodeID SnapNodeA; // valid if snapped to existing node

    // Point B (second click)
    FRailNodeID SnapNodeB; // valid if snapped to existing node

    // Current cursor position (updated on hover)
    FVector CursorPos = FVector::ZeroVector;
    FVector CursorNormal = FVector::UpVector;
    float TangentRotationDeg = 0.f;

    bool bSnapping = false;

    bool RaycastToWorld(const FInputDeviceRay& Ray, FVector& OutPos, FVector& OutNormal) const;
    bool RaycastToNode(const FInputDeviceRay& Ray, FRailNodeID& OutNodeID) const;
    FVector ComputeTangentFromRotation(const FVector& Normal) const;
    FTransform BuildNodeTransform(const FVector& Position, const FVector& SurfaceNormal, const FVector& TangentDir);
};