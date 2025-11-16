// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Game/Lobby/SFW_LobbyGameMode.h"
#include "Core/Game/Lobby/SFW_LobbyGameState.h"
#include "Core/Game/SFW_PlayerState.h"
#include "Core/Game/SFW_PlayerController.h"
#include "Core/Game/SFW_GameInstance.h"
#include "PlayerCharacter/Data/SFW_AgentCatalog.h"

#include "Engine/World.h"
#include "TimerManager.h"

ASFW_LobbyGameMode::ASFW_LobbyGameMode()
{
	// Not using seamless travel for now.
	bUseSeamlessTravel = false;

	// Example route table (adjust or replace from UI with full URLs)
	MapIdToURL.Add(FName("Map_01"), TEXT("/Game/Variant_Horror/Lvl_HorrorTest?listen"));
	MapIdToURL.Add(FName("Map_03"), TEXT("/Game/Core/Game/Levels/Map_3?listen"));

	// Optional allowlist
	AllowedMaps = { FName("Map_01"), FName("Map_03") };
}

void ASFW_LobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>())
	{
		LGS->SetLobbyPhase(ESFWLobbyPhase::Waiting);
		LGS->OnRosterUpdated.AddDynamic(this, &ASFW_LobbyGameMode::HandleRosterUpdated);
		LGS->OnMapChanged.AddDynamic(this, &ASFW_LobbyGameMode::HandleMapChanged);
	}

	bHostRequestedStart = false;
}

void ASFW_LobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Ensure a pawn exists (walkable lobby)
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		RestartPlayer(NewPlayer);
	}

	if (ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>())
	{
		if (ASFW_PlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ASFW_PlayerState>() : nullptr)
		{
			// Reset ready on join (authoritative)
			if (PS->bIsReady) { PS->SetIsReady(false); }

			// Seed agent catalog + default choice on server
			PS->AgentCatalog = AgentCatalog;
			if (AgentCatalog && AgentCatalog->Agents.Num() > 0)
			{
				PS->ServerSetCharacterIndex(0);
			}
			else if (PS->SelectedCharacterID.IsNone())
			{
				PS->SetSelectedCharacterAndVariant(FName(TEXT("Sentinel.AgentA")), PS->SelectedVariantID);
			}

			// Register & broadcast roster
			LGS->RegisterPlayerState(PS);
			LGS->BroadcastRoster();

			// First present player is host
			const int32 NumPlayersNow = GetNumPlayers();
			PS->ServerSetIsHost(NumPlayersNow == 1);
		}
	}
	

	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::Logout(AController* Exiting)
{
	bool bHostLeft = false;
	if (ASFW_PlayerState* LeavingPS = Exiting ? Exiting->GetPlayerState<ASFW_PlayerState>() : nullptr)
	{
		bHostLeft = LeavingPS->GetIsHost();
	}

	Super::Logout(Exiting);

	if (ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>())
	{
		if (ASFW_PlayerState* PS = Exiting ? Exiting->GetPlayerState<ASFW_PlayerState>() : nullptr)
		{
			LGS->UnregisterPlayerState(PS);
		}
		LGS->BroadcastRoster();
	}

	// Promote next available player if host left
	if (bHostLeft)
	{
		bool bPromoted = false;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (ASFW_PlayerState* PS = PC->GetPlayerState<ASFW_PlayerState>())
				{
					if (!bPromoted) { PS->ServerSetIsHost(true);  bPromoted = true; }
					else { PS->ServerSetIsHost(false); }
				}
			}
		}
		// Require new host to press Start again (if rule enabled)
		bHostRequestedStart = false;
	}

	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::HostRequestStart(APlayerController* RequestingPC)
{
	if (!HasAuthority()) return;

	const ASFW_PlayerState* PS = RequestingPC ? RequestingPC->GetPlayerState<ASFW_PlayerState>() : nullptr;
	if (!PS || !PS->GetIsHost())
	{
		UE_LOG(LogTemp, Warning, TEXT("HostRequestStart denied: caller is not the host."));
		return;
	}

	bHostRequestedStart = true;
	EvaluateStartConditions();
}

bool ASFW_LobbyGameMode::IsMapAllowed(FName MapId) const
{
	if (AllowedMaps.Num() == 0) return true;
	return AllowedMaps.Contains(MapId);
}

void ASFW_LobbyGameMode::HandleMapChanged(FName /*NewMap*/)
{
	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::HandleRosterUpdated()
{
	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::EvaluateStartConditions()
{
	if (!HasAuthority()) return;

	ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>();
	if (!LGS) return;

	// Only start from Waiting
	if (LGS->LobbyPhase != ESFWLobbyPhase::Waiting) return;

	// Map must be selected and allowed
	if (LGS->CurrentMap.IsNone() || !IsMapAllowed(LGS->CurrentMap)) return;

	// Player count
	const TArray<APlayerState*>& Players = LGS->PlayerArray;
	const int32 PlayerCount = Players.Num();
	if (PlayerCount < MinPlayersToStart) return;

	// Everyone Ready (if required)
	if (bRequireAllReady)
	{
		for (APlayerState* PSBase : Players)
		{
			const ASFW_PlayerState* PS = Cast<ASFW_PlayerState>(PSBase);
			if (!PS || !PS->bIsReady) return;
		}
	}

	// Host-gate (if required)
	if (bHostMustStart && !bHostRequestedStart) return;

	// Lock and travel (delayed)
	LGS->SetLobbyPhase(ESFWLobbyPhase::Locked);

	const FString URL = ResolveMapURL(LGS->CurrentMap);
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("EvaluateStartConditions: No URL for map '%s'"), *LGS->CurrentMap.ToString());
		LGS->SetLobbyPhase(ESFWLobbyPhase::Waiting);
		return;
	}

	GetWorldTimerManager().SetTimer(
		TravelTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				DoServerTravel();
			}),
		TravelDelaySeconds,
		false
	);
}

FString ASFW_LobbyGameMode::ResolveMapURL(FName MapId) const
{
	if (const FString* Found = MapIdToURL.Find(MapId))
	{
		return *Found;
	}
	return MapId.ToString(); // treat as direct URL/path
}

void ASFW_LobbyGameMode::DoServerTravel()
{
	if (!HasAuthority()) return;

	ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>();
	if (!LGS) return;

	const FString URL = ResolveMapURL(LGS->CurrentMap);
	if (URL.IsEmpty()) return;

	// Snapshot lobby selections before leaving
	SavePreMatchDataForAllPlayers();

	GetWorld()->ServerTravel(URL); // include ?listen in MapIdToURL values
}

void ASFW_LobbyGameMode::SavePreMatchDataForAllPlayers()
{
	if (!HasAuthority()) return;

	USFW_GameInstance* GI = GetGameInstance<USFW_GameInstance>();
	if (!GI) return;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (ASFW_PlayerState* PS = PC->GetPlayerState<ASFW_PlayerState>())
			{
				FSFWPreMatchData Data;
				Data.CharacterID = PS->SelectedCharacterID;
				Data.VariantID = PS->SelectedVariantID;
				
				Data.EquipmentIDs = PS->GetSelectedOptionalIDs();  // optional items chosen in lobby


				// NOTE: Optional equipment IDs will be added once PlayerState supports selection.

				const FString Key = USFW_GameInstance::MakePlayerKey(PS);
				if (!Key.IsEmpty())
				{
					GI->SetPreMatchData(Key, Data);
				}
			}
		}
	}

	// Clear sticky flag after a start is executed.
	bHostRequestedStart = false;
}
