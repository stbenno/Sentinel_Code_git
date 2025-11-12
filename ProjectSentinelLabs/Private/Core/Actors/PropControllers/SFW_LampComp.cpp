// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Actors/PropControllers/SFW_LampComp.h"

#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Core/AnomalySystems/SFW_AnomalyDecisionSystem.h"

USFW_LampComp::USFW_LampComp()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USFW_LampComp::BeginPlay()
{
	Super::BeginPlay();

	bool bBound = false;
	for (TActorIterator<ASFW_AnomalyDecisionSystem> It(GetWorld()); It; ++It)
	{
		if (ASFW_AnomalyDecisionSystem* Sys = *It)
		{
			Sys->OnDecisionBP.AddDynamic(this, &USFW_LampComp::HandleDecisionBP);
			bBound = true;
			break;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s BeginPlay RoomId=%s BoundToDecisionSystem=%d Role=%d"),
		*GetOwner()->GetName(),
		*OwningRoomId.ToString(),
		bBound ? 1 : 0,
		(int32)GetOwnerRole());

	if (GetOwnerRole() == ROLE_Authority)
	{
		RecomputeMode();
	}
	else
	{
		ApplyMode(true);
	}
}

void USFW_LampComp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USFW_LampComp, bHasPower);
	DOREPLIFETIME(USFW_LampComp, bDesiredOn);
	DOREPLIFETIME(USFW_LampComp, FlickerEndSec);
	DOREPLIFETIME(USFW_LampComp, Mode);
}

void USFW_LampComp::OnRep_Mode()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s OnRep_Mode NewMode=%d"),
		*GetOwner()->GetName(),
		(int32)Mode);

	ApplyMode(true);
}

void USFW_LampComp::ServerPlayerToggle_Implementation()
{
	bDesiredOn = !bDesiredOn;
	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s ServerPlayerToggle DesiredOn=%d"),
		*GetOwner()->GetName(),
		bDesiredOn ? 1 : 0);

	RecomputeMode();
}

void USFW_LampComp::OnPowerChanged(bool bPowered)
{
	if (GetOwnerRole() != ROLE_Authority) return;

	bHasPower = bPowered;
	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s OnPowerChanged HasPower=%d"),
		*GetOwner()->GetName(),
		bHasPower ? 1 : 0);

	RecomputeMode();
}

void USFW_LampComp::HandleDecisionBP(const FSFWDecisionPayload& P)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s HandleDecision Type=%d PayRoom=%s OwnRoom=%s Duration=%.2f Role=%d"),
		*GetOwner()->GetName(),
		(int32)P.Type,
		*P.RoomId.ToString(),
		*OwningRoomId.ToString(),
		P.Duration,
		(int32)GetOwnerRole());

	if (P.Type == ESFWDecision::LampFlicker && P.RoomId == OwningRoomId)
	{
		if (GetOwnerRole() == ROLE_Authority)
		{
			FlickerEndSec = GetWorld()->GetTimeSeconds() + P.Duration;
			RecomputeMode();
		}
		else if (Mode == ESFWLampMode::On)
		{
			// client-side local visual flicker if already on
			BP_ApplyLampMode(ESFWLampMode::Flicker, P.Duration);
		}
	}
}

void USFW_LampComp::RecomputeMode()
{
	check(GetOwnerRole() == ROLE_Authority);

	const float Now = GetWorld()->GetTimeSeconds();
	ESFWLampMode NewMode = ESFWLampMode::Off;

	if (!bHasPower)                NewMode = ESFWLampMode::NoPower;
	else if (Now < FlickerEndSec)  NewMode = ESFWLampMode::Flicker;
	else if (bDesiredOn)           NewMode = ESFWLampMode::On;

	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s RecomputeMode Now=%.2f FlickerEnd=%.2f HasPower=%d DesiredOn=%d OldMode=%d NewMode=%d"),
		*GetOwner()->GetName(),
		Now,
		FlickerEndSec,
		bHasPower ? 1 : 0,
		bDesiredOn ? 1 : 0,
		(int32)Mode,
		(int32)NewMode);

	if (NewMode != Mode)
	{
		Mode = NewMode;
		ApplyMode(false);
	}
}

void USFW_LampComp::ApplyMode(bool /*FromRep*/)
{
	float Remaining = 0.f;
	if (Mode == ESFWLampMode::Flicker)
	{
		Remaining = FMath::Max(0.f, FlickerEndSec - GetWorld()->GetTimeSeconds());
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[LampComp] %s ApplyMode Mode=%d Remaining=%.2f"),
		*GetOwner()->GetName(),
		(int32)Mode,
		Remaining);

	BP_ApplyLampMode(Mode, Remaining);
}
