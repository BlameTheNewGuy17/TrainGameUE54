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
    UI_COMMAND(PlaceNodeTool, "Place Node", "Place a rail network node", EUserInterfaceActionType::ToggleButton, FInputChord());
    Commands.Add(NAME_Default, { PlaceNodeTool });
}

#undef LOCTEXT_NAMESPACE