#pragma once
#include "Tools/UEdMode.h"
#include "TrainGameUE54EditorMode.generated.h"

UCLASS()
class UTrainGameUE54EditorMode : public UEdMode
{
    GENERATED_BODY()

public:
    static const FEditorModeID EM_TrainGameUE54EditorModeId;

    UTrainGameUE54EditorMode();
    virtual ~UTrainGameUE54EditorMode();

    virtual void Enter() override;
    virtual void CreateToolkit() override;
    virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
    virtual void ActorSelectionChangeNotify() override;
    virtual void ModeTick(float DeltaTime) override;
};