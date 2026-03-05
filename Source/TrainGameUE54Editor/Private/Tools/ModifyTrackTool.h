#pragma once
#include "BaseTools/SingleClickTool.h"
#include "RailNetworkTypes.h"
#include "ModifyTrackTool.generated.h"

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
    UPROPERTY(EditAnywhere, Category = Options)
    ERailNodeType NodeType = ERailNodeType::Control;

    UPROPERTY(EditAnywhere, Category = Options)
    float SnapToGroundOffset = 0.f;
};

UCLASS()
class UModifyTrackTool : public USingleClickTool
{
    GENERATED_BODY()
public:
    virtual void SetWorld(UWorld* World);
    virtual void Setup() override;
    virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

protected:
    UPROPERTY()
    TObjectPtr<UModifyTrackToolProperties> Properties;

    UWorld* TargetWorld = nullptr;
};