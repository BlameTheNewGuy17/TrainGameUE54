#pragma once
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformProxy.h"
#include "RailNetworkTypes.h"
#include "ModifyTrackTool.generated.h"

UENUM()
enum class ENodeDisplayState : uint8
{
    Default,
    Hovered,
    Selected
};

struct FNodeRenderState
{
    FRailNodeID ID;
    FVector Position;
    float Radius = 50.f;
    ENodeDisplayState State = ENodeDisplayState::Default;
};

UCLASS()
class UModifyTrackToolBuilder : public UInteractiveToolBuilder
{
    GENERATED_BODY()
public:
    virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
    virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS(Transient)
class UModifyTrackToolProperties : public UInteractiveToolPropertySet
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, Category = "Selected Node")
    int32 NodeID = -1;

    UPROPERTY(EditAnywhere, Category = "Selected Node")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Selected Node")
    FRotator Orientation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category = "Selected Node")
    ERailNodeType NodeType = ERailNodeType::Control;

    UPROPERTY(VisibleAnywhere, Category = "Selected Node")
    int32 ConnectedEdgeCount = 0;
};

UCLASS()
class UModifyTrackTool : public UInteractiveTool, public IClickBehaviorTarget, public IHoverBehaviorTarget
{
    GENERATED_BODY()
public:
    virtual void SetWorld(UWorld* World);
    virtual void Setup() override;
    virtual void Shutdown(EToolShutdownType ShutdownType) override;
    virtual void OnTick(float DeltaTime) override;
    virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

    // IClickBehaviorTarget
    virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
    virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

    // IHoverBehaviorTarget
    virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
    virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
    virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
    virtual void OnEndHover() override;

protected:
    void SelectNode(FRailNodeID NodeID);
    void DeselectNode();
    void UpdatePropertiesFromNode();
    bool RaycastToWorld(const FInputDeviceRay& Ray, FVector& OutPos) const;

    UPROPERTY()
    TObjectPtr<UModifyTrackToolProperties> Properties;

    UPROPERTY()
    TObjectPtr<UTransformProxy> TransformProxy;

    UPROPERTY()
    TObjectPtr<UCombinedTransformGizmo> TransformGizmo;

    UWorld* TargetWorld = nullptr;

    FRailNodeID SelectedNodeID;
    FRailNodeID HoveredNodeID;

    bool bHasSelection = false;
    bool bHasHover = false;

    static constexpr float SnapThreshold = 100.f;

    // Visual draw
    TArray<FNodeRenderState> NodeMirror;

    void RebuildMirror();
    bool RaycastToNode(const FInputDeviceRay& Ray, FRailNodeID& OutNodeID) const;

};