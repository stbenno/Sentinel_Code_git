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
	MapIdToURL.Add(FName("Map_01"), TEXT("/Game/Core/Game/Levels/blackout"));
	MapIdToURL.Add(FName("Map_03"), TEXT("/Game/Core/Game/Levels/Map_3"));

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
	bTravelTriggered = false;
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
			if (PS->bIsReady)
			{
				PS->SetIsReady(false);
			}

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
					if (!bPromoted)
					{
						PS->ServerSetIsHost(true);
						bPromoted = true;
					}
					else
					{
						PS->ServerSetIsHost(false);
					}
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
	UE_LOG(LogTemp, Log, TEXT("LobbyGM::HostRequestStart called by %s"),
		RequestingPC ? *RequestingPC->GetName() : TEXT("nullptr"));

	if (!HasAuthority())
	{
		return;
	}

	if (bTravelTriggered)
	{
		UE_LOG(LogTemp, Warning, TEXT("HostRequestStart ignored: travel already triggered."));
		return;
	}

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
	if (AllowedMaps.Num() == 0)
	{
		return true;
	}
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
	UE_LOG(LogTemp, Log, TEXT("LobbyGM::EvaluateStartConditions invoked"));

	if (!HasAuthority())
	{
		return;
	}

	if (bTravelTriggered)
	{
		UE_LOG(LogTemp, Log, TEXT("LobbyGM::EvaluateStartConditions: Travel already triggered, ignoring."));
		return;
	}

	ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>();
	if (!LGS)
	{
		UE_LOG(LogTemp, Warning, TEXT("LobbyGM::EvaluateStartConditions: No LobbyGameState"));
		return;
	}

	// Only start from Waiting
	if (LGS->LobbyPhase != ESFWLobbyPhase::Waiting)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("LobbyGM::EvaluateStartConditions: Phase is %d (not Waiting), ignoring."),
			(int32)LGS->LobbyPhase);
		return;
	}

	// Map must be selected and allowed
	if (LGS->CurrentMap.IsNone() || !IsMapAllowed(LGS->CurrentMap))
	{
		return;
	}

	// Player count
	const TArray<APlayerState*>& Players = LGS->PlayerArray;
	const int32 PlayerCount = Players.Num();
	if (PlayerCount < MinPlayersToStart)
	{
		return;
	}

	// Everyone Ready (if required)
	if (bRequireAllReady)
	{
		for (APlayerState* PSBase : Players)
		{
			const ASFW_PlayerState* PS = Cast<ASFW_PlayerState>(PSBase);
			if (!PS || !PS->bIsReady)
			{
				return;
			}
		}
	}

	// Host-gate (if required)
	if (bHostMustStart && !bHostRequestedStart)
	{
		return;
	}

	// Resolve target URL before we commit
	const FString URL = ResolveMapURL(LGS->CurrentMap);
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("LobbyGM::EvaluateStartConditions: No URL for map '%s'"),
			*LGS->CurrentMap.ToString());
		return;
	}

	// Persist per-player choices for the match
	SavePreMatchDataForAllPlayers();

	// Lock lobby and mark that we've triggered travel
	LGS->SetLobbyPhase(ESFWLobbyPhase::Locked);
	bTravelTriggered = true;

	UE_LOG(LogTemp, Log,
		TEXT("LobbyGM::EvaluateStartConditions -> conditions met, locking lobby and scheduling travel."));

	// Single delayed call to DoServerTravel
	GetWorldTimerManager().ClearTimer(TravelTimerHandle);
	GetWorldTimerManager().SetTimer(
		TravelTimerHandle,
		this,
		&ASFW_LobbyGameMode::DoServerTravel,
		TravelDelaySeconds,
		false);
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
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("LobbyGM::DoServerTravel: No World"));
		return;
	}

	ASFW_LobbyGameState* LobbyGS = GetGameState<ASFW_LobbyGameState>();
	if (!LobbyGS)
	{
		UE_LOG(LogTemp, Warning, TEXT("LobbyGM::DoServerTravel: No LobbyGameState"));
		return;
	}

	FName MapId = LobbyGS->CurrentMap;
	if (MapId.IsNone())
	{
		MapId = FName(TEXT("Map_03")); // fallback
	}

	const FString URL = ResolveMapURL(MapId);

	UE_LOG(LogTemp, Log,
		TEXT("LobbyGM::DoServerTravel: Travelling to URL '%s' (MapId '%s')"),
		*URL, *MapId.ToString());

	bUseSeamlessTravel = false;

	// IMPORTANT: relative travel, not absolute
	World->ServerTravel(URL, /*bAbsolute=*/false);
	// or just: World->ServerTravel(URL);
}




void ASFW_LobbyGameMode::SavePreMatchDataForAllPlayers()
{
	if (!HasAuthority())
	{
		return;
	}

	USFW_GameInstance* GI = GetGameInstance<USFW_GameInstance>();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("SavePreMatchDataForAllPlayers: No GameInstance"));
		return;
	}

	GI->PreMatchCache.Empty();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		if (ASFW_PlayerState* PS = PC->GetPlayerState<ASFW_PlayerState>())
		{
			FSFWPreMatchData Data;
			Data.CharacterID = PS->SelectedCharacterID;
			Data.VariantID = PS->SelectedVariantID;

			const TArray<FName>& SelectedEquip = PS->GetSelectedEquipmentIDs();
			Data.EquipmentIDs = SelectedEquip;

			if (Data.EquipmentIDs.Num() == 0)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SavePreMatchDataForAllPlayers: Player %s has NO selected equipment."),
					*PS->GetPlayerName());
			}

			const FString Key = USFW_GameInstance::MakePlayerKey(PS);
			if (!Key.IsEmpty())
			{
				GI->SetPreMatchData(Key, Data);

				UE_LOG(LogTemp, Log,
					TEXT("SavePreMatchDataForAllPlayers: Saved entry for %s: Char=%s Variant=%s EquipCount=%d"),
					*Key,
					*Data.CharacterID.ToString(),
					*Data.VariantID.ToString(),
					Data.EquipmentIDs.Num());
			}
		}
	}

	bHostRequestedStart = false;
}

