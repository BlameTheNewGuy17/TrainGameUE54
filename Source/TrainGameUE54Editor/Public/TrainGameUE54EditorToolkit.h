#pragma once
#include "Tools/UEdMode.h"
#include "Toolkits/BaseToolkit.h"

class FTrainGameUE54EditorToolkit : public FModeToolkit
{
public:

    FTrainGameUE54EditorToolkit();

    virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;

    virtual FName GetToolkitFName() const override { return FName("TrainGameUE54Editor"); }
    virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("TrainGameUE54EditorToolkit", "DisplayName", "Rail Network Editor"); }
    virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
};