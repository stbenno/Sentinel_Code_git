// Fill out your copyright notice in the Description page of Project Settings.
// SFW_GameMode.cpp

#include "Core/Game/SFW_GameMode.h"
#include "Core/AnomalySystems/SFW_AnomalyController.h"
#include "Core/Game/SFW_GameState.h"
#include "Core/Game/SFW_GameInstance.h"
#include "Core/Game/SFW_PlayerState.h"
#include "PlayerCharacter/Data/SFW_AgentCatalog.h"
#include "Engine/World.h"

ASFW_GameMode::ASFW_GameMode()
{
	// Leave AnomalyControllerClass null so editor forces you to pick BP_SFW_AnomalyController.
	// You can optionally set a default here:
	// AnomalyControllerClass = ASFW_AnomalyController::StaticClass();
}

void ASFW_GameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		StartRound();
	}
}

void ASFW_GameMode::StartRound()
{
	if (!HasAuthority()) return;

	// Reset state
	if (ASFW_GameState* GS = GetGameState<ASFW_GameState>())
	{
		GS->bRoundActive = true;
		GS->AnomalyAggression = 0.f;
	}

	// Clean old controller (hot-reload safety)
	if (IsValid(AnomalyController) && !AnomalyController->IsActorBeingDestroyed())
	{
		AnomalyController->Destroy();
		AnomalyController = nullptr;
	}

	// Require a class to spawn
	if (!AnomalyControllerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("StartRound: AnomalyControllerClass is null. Set it to BP_SFW_AnomalyController in GameMode defaults."));
		return;
	}

	// Spawn and start controller
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
	if (!HasAuthority()) return;

	if (ASFW_GameState* GS = GetGameState<ASFW_GameState>())
	{
		GS->bRoundActive = false;
	}

	if (IsValid(AnomalyController) && !AnomalyController->IsActorBeingDestroyed())
	{
		AnomalyController->Destroy();
		AnomalyController = nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("Round ended. Success=%d"), bSuccess ? 1 : 0);
}

void ASFW_GameMode::FailRound(bool bSuccess)
{
	EndRound(false);
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
