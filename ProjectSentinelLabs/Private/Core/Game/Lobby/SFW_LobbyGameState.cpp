// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/Lobby/SFW_LobbyGameState.h"
#include "Core/Game/SFW_PlayerState.h"
#include "Net/UnrealNetwork.h"

ASFW_LobbyGameState::ASFW_LobbyGameState()
{
	LobbyPhase = ESFWLobbyPhase::Waiting;
}

void ASFW_LobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASFW_LobbyGameState, LobbyPhase);
	DOREPLIFETIME(ASFW_LobbyGameState, CurrentMap);
}

void ASFW_LobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// Bind to any PlayerStates that already exist (can happen in seamless travel etc.)
	RefreshLocalBindings();
	BroadcastRoster();
}

void ASFW_LobbyGameState::SetLobbyPhase(ESFWLobbyPhase NewPhase)
{
	check(HasAuthority());

	if (LobbyPhase != NewPhase)
	{
		LobbyPhase = NewPhase;
		OnRep_LobbyPhase(); // server-side notify too
	}
}

void ASFW_LobbyGameState::OnRep_LobbyPhase()
{
	OnLobbyPhaseChanged.Broadcast(LobbyPhase);
}

void ASFW_LobbyGameState::SetCurrentMap(FName NewMap)
{
	check(HasAuthority());

	if (CurrentMap != NewMap)
	{
		CurrentMap = NewMap;
		OnRep_CurrentMap(); // server-side notify too
	}
}

void ASFW_LobbyGameState::OnRep_CurrentMap()
{
	OnMapChanged.Broadcast(CurrentMap);
}

void ASFW_LobbyGameState::RegisterPlayerState(ASFW_PlayerState* PS)
{
	if (!IsValid(PS)) return;

	BindToPlayerState(PS);
	BroadcastRoster();
}

void ASFW_LobbyGameState::UnregisterPlayerState(ASFW_PlayerState* PS)
{
	if (!IsValid(PS)) return;

	UnbindFromPlayerState(PS);
	BroadcastRoster();
}

void ASFW_LobbyGameState::BroadcastRoster()
{
	OnRosterUpdated.Broadcast();
}

void ASFW_LobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	if (ASFW_PlayerState* PS = Cast<ASFW_PlayerState>(PlayerState))
	{
		BindToPlayerState(PS);
		BroadcastRoster();
	}
}

void ASFW_LobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	if (ASFW_PlayerState* PS = Cast<ASFW_PlayerState>(PlayerState))
	{
		UnbindFromPlayerState(PS);
		BroadcastRoster();
	}
}

void ASFW_LobbyGameState::BindToPlayerState(ASFW_PlayerState* PS)
{
	if (!IsValid(PS)) return;
	if (BoundPlayerStates.Contains(PS)) return;

	PS->OnSelectedCharacterIDChanged.AddDynamic(this, &ASFW_LobbyGameState::HandlePSCharacterIDChanged);
	PS->OnSelectedVariantIDChanged.AddDynamic(this, &ASFW_LobbyGameState::HandlePSVariantIDChanged);
	PS->OnReadyChanged.AddDynamic(this, &ASFW_LobbyGameState::HandlePSReadyChanged);

	BoundPlayerStates.Add(PS);
}

void ASFW_LobbyGameState::UnbindFromPlayerState(ASFW_PlayerState* PS)
{
	if (!IsValid(PS)) return;
	if (!BoundPlayerStates.Contains(PS)) return;

	PS->OnSelectedCharacterIDChanged.RemoveDynamic(this, &ASFW_LobbyGameState::HandlePSCharacterIDChanged);
	PS->OnSelectedVariantIDChanged.RemoveDynamic(this, &ASFW_LobbyGameState::HandlePSVariantIDChanged);
	PS->OnReadyChanged.RemoveDynamic(this, &ASFW_LobbyGameState::HandlePSReadyChanged);

	BoundPlayerStates.Remove(PS);
}

void ASFW_LobbyGameState::RefreshLocalBindings()
{
	// Bind to anyone in the current PlayerArray
	for (APlayerState* PSBase : PlayerArray)
	{
		if (ASFW_PlayerState* PS = Cast<ASFW_PlayerState>(PSBase))
		{
			BindToPlayerState(PS);
		}
	}

	// Unbind anyone who left
	for (auto It = BoundPlayerStates.CreateIterator(); It; ++It)
	{
		ASFW_PlayerState* PS = It->Get();
		if (!IsValid(PS) || !PlayerArray.Contains(PS))
		{
			UnbindFromPlayerState(PS);
		}
	}
}

void ASFW_LobbyGameState::HandlePSCharacterIDChanged(FName /*NewCharacterID*/)
{
	OnRosterUpdated.Broadcast();
}

void ASFW_LobbyGameState::HandlePSVariantIDChanged(FName /*NewVariantID*/)
{
	OnRosterUpdated.Broadcast();
}

void ASFW_LobbyGameState::HandlePSReadyChanged(bool /*bNewIsReady*/)
{
	OnRosterUpdated.Broadcast();
}