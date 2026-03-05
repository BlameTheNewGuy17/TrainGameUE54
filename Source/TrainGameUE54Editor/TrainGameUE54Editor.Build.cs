// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TrainGameUE54Editor : ModuleRules
{
	public TrainGameUE54Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"TrainGameUE54", 
			"Slate", 
			"SlateCore", 
			"UnrealEd",
			"EditorFramework",
			"InteractiveToolsFramework",
			"EditorInteractiveToolsFramework",
			"InputCore",
			"ToolMenus" 
		});
		
	}
}
