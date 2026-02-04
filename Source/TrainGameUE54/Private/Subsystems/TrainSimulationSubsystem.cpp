// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/TrainSimulationSubsystem.h"
#include "Stats/Stats.h"

TStatId UTrainSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTrainSimulationSubsystem, STATGROUP_Tickables);
}

void UTrainSimulationSubsystem::Tick(float DeltaTime)
{
	UE_LOG(LogTemp, Warning, TEXT("Look at me! I'm a ticking World Subsystem!"));
}

