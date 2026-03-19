#include "TrainGameUE54EditorToolkit.h"
#include "TrainGameUE54EditorCommands.h"
#include "Subsystems/RailNetworkSubsystem.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FTrainGameUE54EditorToolkit"

FTrainGameUE54EditorToolkit::FTrainGameUE54EditorToolkit() {}

void FTrainGameUE54EditorToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
    FModeToolkit::Init(InitToolkitHost, InOwningMode);

    const FTrainGameUE54EditorCommands& Commands = FTrainGameUE54EditorCommands::Get();

    UWorld* World = InOwningMode.IsValid() ? InOwningMode->GetWorld() : nullptr;
    URailNetworkSubsystem* RailNetwork = World ? World->GetSubsystem<URailNetworkSubsystem>() : nullptr;

    UE_LOG(LogTemp, Warning, TEXT("Toolkit Init: RailNetwork = %s"), RailNetwork ? TEXT("valid") : TEXT("null"));

    if (RailNetwork)
    {
        GetToolkitCommands()->MapAction(
            Commands.SaveNetwork,
            FExecuteAction::CreateLambda([RailNetwork]()
                {
                    RailNetwork->SaveNetwork(TEXT("DebugSlot"));
                }));

        GetToolkitCommands()->MapAction(
            Commands.LoadNetwork,
            FExecuteAction::CreateLambda([RailNetwork]()
                {
                    RailNetwork->LoadNetwork(TEXT("DebugSlot"));
                }));
    }
}

void FTrainGameUE54EditorToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
    PaletteNames.Add(NAME_Default);
}

#undef LOCTEXT_NAMESPACE