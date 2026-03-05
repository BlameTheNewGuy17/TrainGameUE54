#include "TrainGameUE54EditorCommands.h"

#define LOCTEXT_NAMESPACE "FTrainGameUE54EditorCommands"

FTrainGameUE54EditorCommands::FTrainGameUE54EditorCommands()
    : TCommands<FTrainGameUE54EditorCommands>(
        TEXT("TrainGameUE54Editor"),
        LOCTEXT("TrainGameUE54EditorCommands", "Rail Network Editor"),
        NAME_None,
        FAppStyle::GetAppStyleSetName())
{
}

void FTrainGameUE54EditorCommands::RegisterCommands()
{
    UI_COMMAND(BuildTrackTool, "Build", "Build track nodes and edges", EUserInterfaceActionType::ToggleButton, FInputChord());
    UI_COMMAND(ModifyTrackTool, "Modify", "Select and modify nodes and edges", EUserInterfaceActionType::ToggleButton, FInputChord());
    Commands.Add(NAME_Default, { BuildTrackTool, ModifyTrackTool });
}

#undef LOCTEXT_NAMESPACE