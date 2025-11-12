// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Actors/PropControllers/SFW_LampController.h"
/*#include "Net/UnrealNetwork.h"
#include "Core/AnomalySystems/SFW_AnomalyDecisionSystem.h"
#include "EngineUtils.h"

USFW_LampController::USFW_LampController() { PrimaryComponentTick.bCanEverTick = false; SetIsReplicatedByDefault(true); }

void USFW_LampController::BeginPlay()
{
	Super::BeginPlay();

	// Bind to DecisionSystem on all machines for visuals
	for (TActorIterator<ASFW_AnomalyDecisionSystem> It(GetWorld()); It; ++It)
	{
		ASFW_AnomalyDecisionSystem* Sys = *It;
		if (Sys) { Sys->OnDecisionBP.AddDynamic(this, &USFW_LampController::HandleDecisionBP); break; }
	}
	if (GetOwnerRole() == ROLE_Authority) RecomputeMode();
	else ApplyMode(true); // ensure initial visuals
}

void USFW_LampController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USFW_LampController, bHasPower);
	DOREPLIFETIME(USFW_LampController, bDesiredOn);
	DOREPLIFETIME(USFW_LampController, FlickerEndTime);
	DOREPLIFETIME(USFW_LampController, Mode);
}

void USFW_LampController::OnRep_Mode() { ApplyMode(true); }

void USFW_LampController::ServerPlayerToggle_Implementation()
{
	bDesiredOn = !bDesiredOn;
	RecomputeMode();
}

void USFW_LampController::OnPowerChanged(bool bPowered)
{
	if (GetOwnerRole() != ROLE_Authority) return;
	bHasPower = bPowered;
	RecomputeMode();
}

void USFW_LampController::HandleDecisionBP(const FSFWDecisionPayload& P)
{
	// Visuals react on all clients; authority updates state so late joiners are correct
	if (P.Type == ESFWDecision::LampFlicker && P.RoomId == OwningRoomId)
	{
		if (GetOwnerRole() == ROLE_Authority)
		{
			FlickerEndTime = GetWorld()->GetTimeSeconds() + P.Duration;
			RecomputeMode();
		}
		else if (Mode == ESFWLampMode::On) // clients get immediate visual if already on
		{
			BP_ApplyLampMode(ESFWLampMode::Flicker, P.Duration);
		}
	}
}

void USFW_LampController::RecomputeMode()
{
	check(GetOwnerRole() == ROLE_Authority);
	const float Now = GetWorld()->GetTimeSeconds();

	ESFWLampMode NewMode = ESFWLampMode::Off;
	if (!bHasPower)                        NewMode = ESFWLampMode::NoPower;
	else if (Now < FlickerEndTime)         NewMode = ESFWLampMode::Flicker;
	else if (bDesiredOn)                   NewMode = ESFWLampMode::On;
	else                                   NewMode = ESFWLampMode::Off;

	if (NewMode != Mode)
	{
		Mode = NewMode; // replicates
		ApplyMode(false);
	}
}

void USFW_LampController::ApplyMode(bool /*bFromRep*//*
{
	// Compute remaining flicker time for clients
	float Remaining = 0.f;
	if (Mode == ESFWLampMode::Flicker)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		Remaining = FMath::Max(0.f, FlickerEndTime - Now);
	}
	BP_ApplyLampMode(Mode, Remaining);
}
*/