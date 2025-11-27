// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SFW_LobbyGameState.generated.h"

class ASFW_PlayerState;

/** Lobby phases */
UENUM(BlueprintType)
enum class ESFWLobbyPhase : uint8
{
	Waiting  UMETA(DisplayName = "Waiting"),
	Locked   UMETA(DisplayName = "Locked"),
	InMatch  UMETA(DisplayName = "InMatch")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFWOnRosterUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFWOnLobbyPhaseChanged, ESFWLobbyPhase, NewPhase);
/** Fired when CurrentMap changes (client & server). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFWOnMapChanged, FName, NewMap);

/**
 * GameState for the lobby. Tracks phase and rebroadcasts roster changes.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_LobbyGameState : public AGameState
{
	GENERATED_BODY()

public:
	ASFW_LobbyGameState();

	/** Current lobby phase (replicated). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyPhase, BlueprintReadOnly, Category = "Lobby")
	ESFWLobbyPhase LobbyPhase;

	/** Selected map for the upcoming match (replicated). */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMap, BlueprintReadOnly, Category = "Lobby")
	FName CurrentMap = NAME_None;

	/** Events */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FSFWOnRosterUpdated OnRosterUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FSFWOnLobbyPhaseChanged OnLobbyPhaseChanged;

	/** Fired whenever CurrentMap changes. Bind UI to this. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FSFWOnMapChanged OnMapChanged;

	/** Set the lobby phase (authority). */
	UFUNCTION(BlueprintAuthorityOnly, Category = "Lobby")
	void SetLobbyPhase(ESFWLobbyPhase NewPhase);

	/** Server-only: set the selected map (authoritative). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetCurrentMap(FName NewMap);

	/** Register/unregister a PlayerState so we can listen for its changes (server can call from GameMode). */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RegisterPlayerState(ASFW_PlayerState* PS);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void UnregisterPlayerState(ASFW_PlayerState* PS);

	/** Manually trigger a roster refresh broadcast. */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void BroadcastRoster();

	// Replication list
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** AGameState: called when a PS is added/removed (fires on server and clients). */
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	/** Startup binding pass for already-present PlayerStates. */
	virtual void BeginPlay() override;

	/** RepNotifies */
	UFUNCTION() void OnRep_LobbyPhase();
	UFUNCTION() void OnRep_CurrentMap();

	/** Handlers bound to PlayerState dynamic delegates. */
	UFUNCTION() void HandlePSCharacterIDChanged(FName NewCharacterID);
	UFUNCTION() void HandlePSVariantIDChanged(FName NewVariantID);
	UFUNCTION() void HandlePSReadyChanged(bool bNewIsReady);

private:
	/** Track who we’re bound to (avoid double binds / clean removal). */
	TSet<TWeakObjectPtr<ASFW_PlayerState>> BoundPlayerStates;

	void BindToPlayerState(ASFW_PlayerState* PS);
	void UnbindFromPlayerState(ASFW_PlayerState* PS);
	void RefreshLocalBindings();

	/** Guard so we don't spawn UI while the world is tearing down. */
	bool CanBroadcastToUI() const;
};
