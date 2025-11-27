// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Game/SFW_GameState.h"
#include "Net/UnrealNetwork.h"

#include "GameFramework/PlayerState.h"
#include "PlayerCharacter/SFW_PlayerBase.h"
#include "Core/Components/SFW_EquipmentManagerComponent.h"
#include "Core/Actors/SFW_EMFDevice.h"

ASFW_GameState::ASFW_GameState()
{
	// round defaults
	bRoundActive = false;
	RoundSeed = 0;
	RoundStartTime = 0.f;

	// pacing defaults
	AnomalyAggression = 0.f;
	ActiveClass = EAnomalyClass::Binder;

	// room refs
	BaseRoom = nullptr;
	RiftRoom = nullptr;

	// evidence defaults
	bEvidenceWindowActive = false;
	EvidenceWindowStartTime = -1.f;
	EvidenceWindowDurationSec = 0.f;
	CurrentEvidenceType = 0;

	// binder scare budget
	BinderDoorScareBudget = 2; // at most two global face-slams per match

	// radio defaults
	bRadioJammed = false;
	RadioIntegrity = 1.0f;
}

void ASFW_GameState::BeginRound(float Now, int32 Seed)
{
	RoundStartTime = Now;
	RoundSeed = Seed;
	RoundEndTime = -1.f;
	RoundDuration = 0.f;
	RoundResult = ESFWRoundResult::None;

	bRoundInProgress = true;
	bRoundActive = true;
}

void ASFW_GameState::EndRound(float Now)
{
	RoundEndTime = Now;
	RoundDuration = FMath::Max(0.f, RoundEndTime - RoundStartTime);

	bRoundInProgress = false;
	bRoundActive = false;
}


void ASFW_GameState::OnRep_EvidenceWindow()
{
	// clients can update HUD/sfx here if desired
}

void ASFW_GameState::Server_StartEvidenceWindow_Implementation(int32 EvidenceType, float DurationSec)
{
	if (!HasAuthority())
	{
		return;
	}

	bEvidenceWindowActive = true;
	EvidenceWindowStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	EvidenceWindowDurationSec = FMath::Max(0.f, DurationSec);
	CurrentEvidenceType = EvidenceType;

	UE_LOG(LogTemp, Log, TEXT("[GameState] StartEvidenceWindow type=%d dur=%.2fs"),
		EvidenceType,
		EvidenceWindowDurationSec);

	// kick gear immediately
	Server_TriggerEvidence(EvidenceType);
}

void ASFW_GameState::Server_EndEvidenceWindow_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bEvidenceWindowActive)
	{
		return;
	}

	bEvidenceWindowActive = false;
	UE_LOG(LogTemp, Log, TEXT("[GameState] EndEvidenceWindow"));
}

void ASFW_GameState::Server_TriggerEvidence_Implementation(int32 EvidenceType)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[GameState] Server_TriggerEvidence type=%d START. PlayerArray size=%d"),
		EvidenceType,
		PlayerArray.Num());

	for (APlayerState* PS : PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		APawn* Pawn = PS->GetPawn();
		ASFW_PlayerBase* PlayerChar = Cast<ASFW_PlayerBase>(Pawn);
		if (!PlayerChar)
		{
			continue;
		}

		USFW_EquipmentManagerComponent* Equip =
			PlayerChar->FindComponentByClass<USFW_EquipmentManagerComponent>();
		if (!Equip)
		{
			continue;
		}

		// EvidenceType 1 == EMF spike
		if (EvidenceType == 1)
		{
			ASFW_EMFDevice* EMF = Equip->FindEMF();
			if (EMF)
			{
				UE_LOG(LogTemp, Log, TEXT("[GameState] Triggering EMF spike on %s"), *EMF->GetName());
				EMF->Server_SetActive(true);
			}
		}

		// TODO: add thermometer, UV, etc for other evidence types
	}

	UE_LOG(LogTemp, Log, TEXT("[GameState] Server_TriggerEvidence END"));
}

// binder scare budget consume
void ASFW_GameState::ConsumeBinderDoorScare()
{
	if (!HasAuthority())
	{
		return;
	}
	if (BinderDoorScareBudget > 0)
	{
		BinderDoorScareBudget--;
	}
}

// radio controls
void ASFW_GameState::Server_SetRadioJammed(bool bJammed)
{
	if (HasAuthority())
	{
		bRadioJammed = bJammed;
	}
}

void ASFW_GameState::Server_SetRadioIntegrity(float NewIntegrity)
{
	if (HasAuthority())
	{
		RadioIntegrity = FMath::Clamp(NewIntegrity, 0.0f, 1.0f);

		// simple rule. if integrity is almost dead then jam
		if (RadioIntegrity <= 0.1f)
		{
			bRadioJammed = true;
		}
	}
}

// replication
void ASFW_GameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Round
	DOREPLIFETIME(ASFW_GameState, bRoundActive);
	DOREPLIFETIME(ASFW_GameState, RoundSeed);
	DOREPLIFETIME(ASFW_GameState, RoundStartTime);
	DOREPLIFETIME(ASFW_GameState, bRoundInProgress);
	DOREPLIFETIME(ASFW_GameState, RoundEndTime);
	DOREPLIFETIME(ASFW_GameState, RoundResult);
	DOREPLIFETIME(ASFW_GameState, RoundDuration);

	// Extraction
	DOREPLIFETIME(ASFW_GameState, bExtractionCountdownActive);
	DOREPLIFETIME(ASFW_GameState, ExtractionEndTime);

	// Anomaly / rooms
	DOREPLIFETIME(ASFW_GameState, AnomalyAggression);
	DOREPLIFETIME(ASFW_GameState, ActiveClass);
	DOREPLIFETIME(ASFW_GameState, BaseRoom);
	DOREPLIFETIME(ASFW_GameState, RiftRoom);

	// Evidence
	DOREPLIFETIME(ASFW_GameState, bEvidenceWindowActive);
	DOREPLIFETIME(ASFW_GameState, EvidenceWindowStartTime);
	DOREPLIFETIME(ASFW_GameState, EvidenceWindowDurationSec);
	DOREPLIFETIME(ASFW_GameState, CurrentEvidenceType);

	// Binder
	DOREPLIFETIME(ASFW_GameState, BinderDoorScareBudget);

	// Radio
	DOREPLIFETIME(ASFW_GameState, bRadioJammed);
	DOREPLIFETIME(ASFW_GameState, RadioIntegrity);
}

