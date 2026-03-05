// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrainGameUE54EditorModule.h"
#include "TrainGameUE54EditorCommands.h"

#define LOCTEXT_NAMESPACE "FTrainGameUE54EditorModule"

IMPLEMENT_GAME_MODULE( FTrainGameUE54EditorModule, TrainGameUE54Editor);

void FTrainGameUE54EditorModule::StartupModule()
{ 
	FTrainGameUE54EditorCommands::Register();
}

void FTrainGameUE54EditorModule::ShutdownModule()
{
	FTrainGameUE54EditorCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE