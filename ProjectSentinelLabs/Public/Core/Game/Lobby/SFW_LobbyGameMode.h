// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SFW_LobbyGameMode.generated.h"

class ASFW_LobbyGameState;
class ASFW_PlayerState;
class USFW_AgentCatalog;
class USFW_GameInstance;

/**
 * Lobby GameMode: manages host designation, roster/map changes, and starting the match.
 * Supports 1–4 players, optional host-gate, and delayed server travel.
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

	/** Host presses "Start" in the lobby UI (Server authority). */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void HostRequestStart(APlayerController* RequestingPC);

protected:
	/** Re-checks whether we can leave the lobby (map + readiness + host-gate). */
	UFUNCTION()
	void EvaluateStartConditions();

	/** Roster/ready changed -> re-evaluate. */
	UFUNCTION()
	void HandleRosterUpdated();

	/** Current map changed -> re-evaluate. */
	UFUNCTION()
	void HandleMapChanged(FName NewMap);

	/** Optional allowlist validation for MapId. */
	bool IsMapAllowed(FName MapId) const;

	/** Resolve MapId to travel URL. If no mapping, MapId is used as the URL. */
	FString ResolveMapURL(FName MapId) const;

	/** Execute server travel after TravelDelaySeconds. */
	UFUNCTION()
	void DoServerTravel();

	/** Persist per-player pre-match selections to GameInstance before traveling. */
	void SavePreMatchDataForAllPlayers();

protected:
	// ---------- Start rules ----------

	/** Minimum players required to start. Set to 1 to allow solo. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Rules")
	int32 MinPlayersToStart = 1;

	/** Require all present players to be Ready. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Rules")
	bool bRequireAllReady = true;

	/** Require the host to press Start. If false, auto-start when ready. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Rules")
	bool bHostMustStart = true;

	/** Sticky flag for this session; set by HostRequestStart(). */
	UPROPERTY(VisibleAnywhere, Category = "Lobby|Rules")
	bool bHostRequestedStart = false;

	/** Delay before travel so clients can display a transition overlay. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Rules")
	float TravelDelaySeconds = 1.0f;

	/** Once true, travel is locked in; prevents double-trigger. */
	UPROPERTY()
	bool bTravelTriggered = false;

	FTimerHandle TravelTimerHandle;

	// ---------- Maps ----------

	/** Optional allowlist; if empty, any MapId/URL is accepted. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Maps")
	TSet<FName> AllowedMaps;

	/** Optional routing table (Id -> URL). If empty, MapId is used directly. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Maps")
	TMap<FName, FString> MapIdToURL;

	// ---------- Agents / Characters ----------

	/** Assign your DA_AgentCatalog (used to seed player selection on join). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agents")
	TObjectPtr<USFW_AgentCatalog> AgentCatalog = nullptr;
};
