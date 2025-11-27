// Fill out your copyright notice in the Description page of Project Settings.

// SFW_GameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SFW_GameMode.generated.h"

class ASFW_AnomalyController;
class USFW_AgentCatalog;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_GameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ASFW_GameMode();

	UFUNCTION(BlueprintCallable)
	void StartRound();

	UFUNCTION(BlueprintCallable)
	void EndRound(bool bSuccess);

	UFUNCTION(BlueprintCallable)
	void FailRound(bool bSuccess);

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	void BuildAndStageTeamLoadout();

	/** Class to spawn for the anomaly controller (set this to the BP in defaults). */
	UPROPERTY(EditDefaultsOnly, Category = "Anomaly")
	TSubclassOf<ASFW_AnomalyController> AnomalyControllerClass;

	/** Runtime instance of the anomaly controller. */
	UPROPERTY()
	ASFW_AnomalyController* AnomalyController = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agents")
	TObjectPtr<USFW_AgentCatalog> AgentCatalog = nullptr;

	// --- Round end / return-to-lobby ---

	/** Delay before we bounce back to lobby after round end. */
	UPROPERTY(EditDefaultsOnly, Category = "Round")
	float PostRoundReturnDelay = 5.0f;

	/** Map URL to return to for lobby. Must be a valid ServerTravel URL. */
	UPROPERTY(EditDefaultsOnly, Category = "Round")
	FString LobbyMapURL;

	/** Guard so EndRound only runs once. */
	UPROPERTY()
	bool bRoundEnded = false;

	/** Timer for the post-round return to lobby. */
	FTimerHandle RoundEndTravelTimerHandle;

	/** Actually performs the travel back to lobby. */
	UFUNCTION()
	void HandleReturnToLobby();

	UFUNCTION(BlueprintCallable, Category = "Flow")
	void ReturnToLobby();
};
