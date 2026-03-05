#include "TrainGameUE54EditorMode.h"
#include "TrainGameUE54EditorToolkit.h"
#include "TrainGameUE54EditorCommands.h"
#include "Tools/PlaceNodeTool.h"
#include "Tools/BuildTrackTool.h"
#include "Tools/ModifyTrackTool.h"
#include "InteractiveToolManager.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/RailNetworkSubsystem.h"

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

void UTrainGameUE54EditorMode::ModeTick(float DeltaTime)
{
    UEdMode::ModeTick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World) return;

    URailNetworkSubsystem* RailNetwork = World->GetSubsystem<URailNetworkSubsystem>();
    if (!RailNetwork) return;

    RailNetwork->ClearDebugDraw();
    RailNetwork->DebugDrawRailNetwork(-1.f, 2.f);
}

void UTrainGameUE54EditorMode::Enter()
{
    UEdMode::Enter();
    // Tools are registered here

    const FTrainGameUE54EditorCommands& Commands = FTrainGameUE54EditorCommands::Get();

    RegisterTool(Commands.BuildTrackTool, TEXT("BuildTrackTool"), NewObject<UBuildTrackToolBuilder>(this));
    RegisterTool(Commands.ModifyTrackTool, TEXT("ModifyTrackTool"), NewObject<UModifyTrackToolBuilder>(this));

    GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("BuildTrackTool"));
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