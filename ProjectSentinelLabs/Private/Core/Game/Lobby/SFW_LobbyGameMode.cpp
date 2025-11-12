// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/Lobby/SFW_LobbyGameMode.h"
#include "Core/Game/Lobby/SFW_LobbyGameState.h"
#include "Core/Game/SFW_PlayerState.h"
#include "Core/Game/SFW_PlayerController.h"
#include "Net/UnrealNetwork.h"
// --- NEW ---
#include "PlayerCharacter/Data/SFW_AgentCatalog.h" // type reference only
#include "Core/Game/SFW_GameInstance.h"
// --- NEW END ---

ASFW_LobbyGameMode::ASFW_LobbyGameMode()
{
	// Explicit: we are NOT using seamless travel for now.
	bUseSeamlessTravel = false;

	// Example entries (optional) — replace with your paths or pass full URLs from the widget.
	// MapIdToURL.Add(FName("Map_01"), TEXT("/Game/Maps/Map_01?listen"));
	// MapIdToURL.Add(FName("Map_02"), TEXT("/Game/Maps/Map_02?listen"));
	MapIdToURL.Add(FName("Map_01"), TEXT("/Game/Variant_Horror/Lvl_HorrorTest?listen"));
	MapIdToURL.Add(FName("Map_03"), TEXT("/Game/Core/Game/Levels/Map_3?listen"));
}

void ASFW_LobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>())
	{
		// Start unlocked
		LGS->SetLobbyPhase(ESFWLobbyPhase::Waiting);

		// Re-evaluate whenever roster/ready or map selection changes
		LGS->OnRosterUpdated.AddDynamic(this, &ASFW_LobbyGameMode::HandleRosterUpdated);
		LGS->OnMapChanged.AddDynamic(this, &ASFW_LobbyGameMode::HandleMapChanged);
	}
}

void ASFW_LobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Spawn a pawn if needed (so players can walk around in the lobby)
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		UE_LOG(LogTemp, Verbose, TEXT("PostLogin: %s has no pawn, calling RestartPlayer"), *GetNameSafe(NewPlayer));
		RestartPlayer(NewPlayer);
	}

	if (ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>())
	{
		if (ASFW_PlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ASFW_PlayerState>() : nullptr)
		{
			// Reset ready on join (authoritative)
			if (PS->bIsReady) { PS->SetIsReady(false); }

			// --- NEW: hand out catalog & default to Agent A (index 0) ---
			PS->AgentCatalog = AgentCatalog; // replicates to client
			if (AgentCatalog && AgentCatalog->Agents.Num() > 0)
			{
				// Ensures CharacterIndex wraps & SelectedCharacterID is set on server
				PS->ServerSetCharacterIndex(0);
			}
			else
			{
				// Fallback if catalog missing: make sure we at least have a valid ID
				if (PS->SelectedCharacterID.IsNone())
				{
					PS->SetSelectedCharacterAndVariant(FName(TEXT("Sentinel.AgentA")), PS->SelectedVariantID);
				}
			}
			// --- NEW END ---

			// Register for roster, broadcast
			LGS->RegisterPlayerState(PS);
			LGS->BroadcastRoster();

			// Host assignment: first player is host; others are not
			const int32 NumPlayersNow = GetNumPlayers();
			PS->ServerSetIsHost(NumPlayersNow == 1);
		}
	}

	// New presence might satisfy start conditions
	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::Logout(AController* Exiting)
{
	// Track if host leaves
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
	}

	// Re-evaluate after someone leaves
	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::HandleRosterUpdated()
{
	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::DoServerTravel()
{
	if (!HasAuthority()) return;

	ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>();
	if (!LGS) return;

	const FString URL = ResolveMapURL(LGS->CurrentMap);
	if (URL.IsEmpty()) return;


	SavePreMatchDataForAllPlayers();

	GetWorld()->ServerTravel(URL); // include ?listen if you’re hosting
}

void ASFW_LobbyGameMode::HandleMapChanged(FName /*NewMap*/)
{
	EvaluateStartConditions();
}

void ASFW_LobbyGameMode::EvaluateStartConditions()
{
	if (!HasAuthority()) return;

	ASFW_LobbyGameState* LGS = GetGameState<ASFW_LobbyGameState>();
	if (!LGS) return;

	// Only start from Waiting (avoid repeated triggers)
	if (LGS->LobbyPhase != ESFWLobbyPhase::Waiting)
	{
		return;
	}

	// Require a selected map
	if (LGS->CurrentMap.IsNone())
	{
		return;
	}

	// Require at least MinPlayersToStart (default 1, so solo works)
	const TArray<APlayerState*>& Players = LGS->PlayerArray;
	const int32 PlayerCount = Players.Num();
	if (PlayerCount < MinPlayersToStart)
	{
		return;
	}

	// Everyone present must be ready
	for (APlayerState* PSBase : Players)
	{
		const ASFW_PlayerState* PS = Cast<ASFW_PlayerState>(PSBase);
		if (!PS || !PS->bIsReady)
		{
			return; // someone is not ready
		}
	}

	// Passed all checks — lock lobby and then travel after a short delay
	LGS->SetLobbyPhase(ESFWLobbyPhase::Locked);

	const FString URL = ResolveMapURL(LGS->CurrentMap);
	if (URL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("EvaluateStartConditions: No URL for map '%s'"), *LGS->CurrentMap.ToString());
		LGS->SetLobbyPhase(ESFWLobbyPhase::Waiting);
		return;
	}

	// cache URL in a lambda-safe way:
	const FString TravelURL = URL;
	UE_LOG(LogTemp, Log, TEXT("Starting match with %d player(s). Traveling to: %s in %.2fs"), PlayerCount, *TravelURL, TravelDelaySeconds);

	// set a timer so clients get the OnRep_LobbyPhase and show overlay
	GetWorldTimerManager().SetTimer(
		TravelTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, TravelURL]()
			{
				DoServerTravel(); // uses a member to actually travel
				// or directly: GetWorld()->ServerTravel(TravelURL);
			}),
		TravelDelaySeconds,
		false
	);
}

FString ASFW_LobbyGameMode::ResolveMapURL(FName MapId) const
{
	// Prefer explicit routing table; otherwise treat MapId as a direct path/URL
	if (const FString* Found = MapIdToURL.Find(MapId))
	{
		return *Found;
	}
	return MapId.ToString();
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
				// Data.EquipmentIDs (optional later)

				const FString Key = USFW_GameInstance::MakePlayerKey(PS);
				if (!Key.IsEmpty())
				{
					GI->SetPreMatchData(Key, Data);
				}
			}
		}
	}
}
// --- NEW END ---


