#include "TrainGameUE54EditorMode.h"
#include "TrainGameUE54EditorToolkit.h"
#include "TrainGameUE54EditorCommands.h"
#include "Tools/PlaceNodeTool.h"
#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "FTrainGameUE54EditorMode"

const FEditorModeID UTrainGameUE54EditorMode::EM_TrainGameUE54EditorModeId = TEXT("EM_TrainGameUE54EditorMode");

UTrainGameUE54EditorMode::UTrainGameUE54EditorMode()
{
    Info = FEditorModeInfo(
        UTrainGameUE54EditorMode::EM_TrainGameUE54EditorModeId,
        LOCTEXT("TrainGameUE54EditorModeName", "Rail Network Editor"),
        FSlateIcon(),
        true);
}

UTrainGameUE54EditorMode::~UTrainGameUE54EditorMode()
{
}

void UTrainGameUE54EditorMode::ActorSelectionChangeNotify()
{
}

void UTrainGameUE54EditorMode::Enter()
{
    UEdMode::Enter();
    // Tools are registered here

    const FTrainGameUE54EditorCommands& Commands = FTrainGameUE54EditorCommands::Get();
    RegisterTool(Commands.PlaceNodeTool, TEXT("PlaceNodeTool"), NewObject<UPlaceNodeToolBuilder>(this));

    GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("PlaceNodeTool"));
}

void UTrainGameUE54EditorMode::CreateToolkit()
{
    Toolkit = MakeShareable(new FTrainGameUE54EditorToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UTrainGameUE54EditorMode::GetModeCommands() const
{
    return FTrainGameUE54EditorCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE