#include "TrainGameUE54EditorToolkit.h"
#include "TrainGameUE54EditorCommands.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FTrainGameUE54EditorToolkit"

FTrainGameUE54EditorToolkit::FTrainGameUE54EditorToolkit() {}

void FTrainGameUE54EditorToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
    FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FTrainGameUE54EditorToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
    PaletteNames.Add(NAME_Default);
}

#undef LOCTEXT_NAMESPACE