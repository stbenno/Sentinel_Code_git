// Fill out your copyright notice in the Description page of Project Settings.
// SFW_GameMode.cpp

#include "Core/Game/SFW_GameMode.h"
#include "Core/AnomalySystems/SFW_AnomalyController.h"
#include "Core/Game/SFW_GameState.h"
#include "Core/Game/SFW_GameInstance.h"
#include "Core/Game/SFW_PlayerState.h"
#include "PlayerCharacter/Data/SFW_AgentCatalog.h"
#include "Engine/World.h"
#include "Core/Game/SFW_StagingTypes.h"
#include "Core/Game/SFW_StagingArea.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h" // for GetWorldTimerManager / FTimerHandle

ASFW_GameMode::ASFW_GameMode()
{
	// Leave AnomalyControllerClass null so editor forces you to pick BP_SFW_AnomalyController.
	// You can optionally set a default here:
	// AnomalyControllerClass = ASFW_AnomalyController::StaticClass();

	// --- Round / lobby defaults ---
	// This should match the path you use in LobbyGameMode.
	LobbyMapURL = TEXT("/Game/Core/Game/Levels/Lvl_Lobby");
}

void ASFW_GameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BuildAndStageTeamLoadout();
		StartRound();
	}
}

void ASFW_GameMode::StartRound()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	if (ASFW_GameState* GS = GetGameState<ASFW_GameState>())
	{
		const int32 Seed = FMath::Rand(); // later you can make this deterministic
		GS->BeginRound(Now, Seed);
		GS->AnomalyAggression = 0.f;
	}

	// Clean old controller (hot-reload safety)
	if (IsValid(AnomalyController) && !AnomalyController->IsActorBeingDestroyed())
	{
		AnomalyController->Destroy();
		AnomalyController = nullptr;
	}

	if (!AnomalyControllerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("StartRound: AnomalyControllerClass is null. Set it to BP_SFW_AnomalyController in GameMode defaults."));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AnomalyController = GetWorld()->SpawnActor<ASFW_AnomalyController>(
		AnomalyControllerClass, FTransform::Identity, Params);

	if (AnomalyController)
	{
		AnomalyController->StartRound();
		UE_LOG(LogTemp, Warning, TEXT("StartRound: AnomalyController Spawned + Started (%s)."),
			*AnomalyController->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StartRound: Failed to spawn AnomalyController."));
	}
}


void ASFW_GameMode::EndRound(bool bSuccess)
{
	if (!HasAuthority())
	{
		return;
	}

	// One-shot guard so we don't spam ServerTravel / summaries
	if (bRoundEnded)
	{
		UE_LOG(LogTemp, Verbose, TEXT("EndRound: already ended, ignoring duplicate call"));
		return;
	}
	bRoundEnded = true;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	if (ASFW_GameState* GS = GetGameState<ASFW_GameState>())
	{
		// Mark round timings and result
		GS->EndRound(Now);
		GS->RoundResult = bSuccess ? ESFWRoundResult::Success : ESFWRoundResult::Fail;

		// Persist summary into GameInstance so lobby can read it
		if (USFW_GameInstance* GI = GetGameInstance<USFW_GameInstance>())
		{
			FSFWLastRoundSummary Summary;
			Summary.Result = GS->RoundResult;
			Summary.DurationSeconds = GS->RoundDuration;                          // assuming GS computes this
			Summary.MapId = World ? FName(*World->GetMapName()) : NAME_None;
			Summary.EndTimeSeconds = Now;

			GI->SetLastRoundSummary(Summary);
		}
	}

	// Clean up anomaly controller
	if (IsValid(AnomalyController) && !AnomalyController->IsActorBeingDestroyed())
	{
		AnomalyController->Destroy();
		AnomalyController = nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("Round ended. Success=%d"), bSuccess ? 1 : 0);

	// Schedule the actual travel back to lobby
	if (World)
	{
		World->GetTimerManager().ClearTimer(RoundEndTravelTimerHandle);
		World->GetTimerManager().SetTimer(
			RoundEndTravelTimerHandle,
			this,
			&ASFW_GameMode::HandleReturnToLobby,
			PostRoundReturnDelay,
			false
		);
	}
}

void ASFW_GameMode::FailRound(bool /*bSuccess*/)
{
	// For now we ignore the parameter and always treat this as failure
	EndRound(false);
}

void ASFW_GameMode::HandleReturnToLobby()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleReturnToLobby: No World"));
		return;
	}

	if (LobbyMapURL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleReturnToLobby: LobbyMapURL is empty"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("HandleReturnToLobby: ServerTravel to '%s'"), *LobbyMapURL);

	// Keep existing host/port; just change level
	bUseSeamlessTravel = false;
	World->ServerTravel(LobbyMapURL); // default bAbsolute = false
}


void ASFW_GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (!HasAuthority() || !NewPlayer) return;

	if (ASFW_PlayerState* PS = NewPlayer->GetPlayerState<ASFW_PlayerState>())
	{
		PS->AgentCatalog = AgentCatalog;

		if (USFW_GameInstance* GI = GetGameInstance<USFW_GameInstance>())
		{
			const FString Key = USFW_GameInstance::MakePlayerKey(PS);
			FSFWPreMatchData Saved;
			if (GI->GetPreMatchData(Key, Saved))
			{
				PS->ServerSetCharacterByID(Saved.CharacterID);
				PS->SetSelectedCharacterAndVariant(PS->SelectedCharacterID, Saved.VariantID);
			}
		}
	}
}

void ASFW_GameMode::BuildAndStageTeamLoadout()
{
	UE_LOG(LogTemp, Log, TEXT("BuildAndStageTeamLoadout: called on server"));

	if (!HasAuthority())
	{
		return;
	}

	USFW_GameInstance* GI = GetGameInstance<USFW_GameInstance>();
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildAndStageTeamLoadout: No GameInstance"));
		return;
	}

	// 1) Aggregate equipment IDs across all pre-match entries
	TMap<FName, int32> AggregatedCounts;

	for (const TPair<FString, FSFWPreMatchData>& Pair : GI->PreMatchCache)
	{
		const FSFWPreMatchData& Data = Pair.Value;

		for (const FName& EquipId : Data.EquipmentIDs)
		{
			if (!EquipId.IsNone())
			{
				AggregatedCounts.FindOrAdd(EquipId)++;
			}
		}
	}

	// 2) Convert into staging requests
	TArray<FSFWStageItemRequest> StageRequests;
	StageRequests.Reserve(AggregatedCounts.Num());

	for (const TPair<FName, int32>& Pair : AggregatedCounts)
	{
		FSFWStageItemRequest Req;
		Req.ItemId = Pair.Key;
		Req.TotalCount = Pair.Value;

		StageRequests.Add(Req);

		UE_LOG(LogTemp, Log, TEXT("StageRequest: %s x%d"),
			*Req.ItemId.ToString(),
			Req.TotalCount);
	}

	// 3) Find the staging area actor in this map
	ASFW_StagingArea* StagingArea = Cast<ASFW_StagingArea>(
		UGameplayStatics::GetActorOfClass(this, ASFW_StagingArea::StaticClass())
	);

	if (!StagingArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildAndStageTeamLoadout: No ASFW_StagingArea found in world."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BuildAndStageTeamLoadout: Calling StageItems on %s with %d entries."),
		*StagingArea->GetName(),
		StageRequests.Num());

	StagingArea->StageItems(StageRequests);
}

void ASFW_GameMode::ReturnToLobby()
{
	HandleReturnToLobby();
}
