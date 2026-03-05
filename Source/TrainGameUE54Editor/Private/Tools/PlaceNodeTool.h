#pragma once
#include "BaseTools/SingleClickTool.h"
#include "RailNetworkTypes.h"
#include "PlaceNodeTool.generated.h"

UCLASS()
class UPlaceNodeToolBuilder : public UInteractiveToolBuilder
{
    GENERATED_BODY()
public:
    virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
    virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS(Transient)
class UPlaceNodeToolProperties : public UInteractiveToolPropertySet
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = Options)
    ERailNodeType NodeType = ERailNodeType::Control;

    UPROPERTY(EditAnywhere, Category = Options)
    float SnapToGroundOffset = 0.f;
};

UCLASS()
class UPlaceNodeTool : public USingleClickTool
{
    GENERATED_BODY()
public:
    virtual void SetWorld(UWorld* World);
    virtual void Setup() override;
    virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

protected:
    UPROPERTY()
    TObjectPtr<UPlaceNodeToolProperties> Properties;

    UWorld* TargetWorld = nullptr;
};