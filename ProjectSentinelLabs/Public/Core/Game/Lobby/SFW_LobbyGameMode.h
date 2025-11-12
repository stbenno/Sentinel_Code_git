// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SFW_LobbyGameMode.generated.h"

class ASFW_LobbyGameState;
class ASFW_PlayerState;
// --- NEW: forward declare catalog ---
class USFW_AgentCatalog;
class USFW_GameInstance;

/**
 * Lobby GameMode: manages host designation, roster updates, and starting the match
 * when a map is selected and all present players are Ready (supports 1–4 players).
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_LobbyGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ASFW_LobbyGameMode();

	// AGameModeBase
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	/** Re-checks whether we can leave the lobby (map selected + all present players are ready). */
	UFUNCTION() void EvaluateStartConditions();

	/** Roster/ready changed -> re-evaluate. */
	UFUNCTION() void HandleRosterUpdated();

	// Delay before traveling so clients can show the overlay
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	float TravelDelaySeconds = 1.0f;

	FTimerHandle TravelTimerHandle;

	UFUNCTION()
	void DoServerTravel();

	/** Current map changed -> re-evaluate. */
	UFUNCTION() void HandleMapChanged(FName NewMap);

	/** Resolve a MapId to a travel URL (or return the MapId if you pass full paths). */
	FString ResolveMapURL(FName MapId) const;

	/** Minimum players required to start (set to 1 to allow solo). */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 MinPlayersToStart = 1;

	/** Optional routing table (Id -> URL). If empty, the MapId string is used directly. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TMap<FName, FString> MapIdToURL;

	// --- NEW: assign in BP to your DA_AgentCatalog asset ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agents")
	TObjectPtr<USFW_AgentCatalog> AgentCatalog = nullptr;
	// --- NEW END ---

	// --- NEW: save everyone’s selection into GameInstance before traveling
	void SavePreMatchDataForAllPlayers();
};