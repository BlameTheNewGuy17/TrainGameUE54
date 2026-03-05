#pragma once
#include "Framework/Commands/Commands.h"

class FTrainGameUE54EditorCommands : public TCommands<FTrainGameUE54EditorCommands>
{
public:
    FTrainGameUE54EditorCommands();

    virtual void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> PlaceNodeTool;

    static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands()
    {
        return FTrainGameUE54EditorCommands::Get().Commands;
    }

protected:
    TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};