// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "SFW_ExtractionVolume.generated.h"

class ASFW_GameMode;
class APlayerController;

/**
 * Extraction volume:
 * - Tracks which (living) players are inside.
 * - When ALL living players are inside, starts a countdown.
 * - If anyone leaves, cancels the countdown.
 * - When countdown completes, tells GameMode to EndRound(true).
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_ExtractionVolume : public ATriggerBox
{
	GENERATED_BODY()

public:
	ASFW_ExtractionVolume();

protected:
	virtual void BeginPlay() override;

	/** Pawn enters extraction zone. */
	UFUNCTION()
	void OnBoxBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/** Pawn leaves extraction zone. */
	UFUNCTION()
	void OnBoxEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/** Re-checks whether we should start/cancel extraction. */
	void ReevaluateExtractionState();

	/** True if all *living* players (with pawns) are inside. */
	bool AreAllLivingPlayersInside() const;

	/** Count of living players (with pawns) for logging. */
	int32 GetLivingPlayerCount() const;

	/** Start the extraction countdown timer. */
	void StartExtractionCountdown();

	/** Cancel any active extraction countdown. */
	void CancelExtractionCountdown();

	/** Called when countdown finishes and conditions are still valid. */
	UFUNCTION()
	void HandleExtractionComplete();

protected:
	/** How long everyone must stand in the zone before extraction. */
	UPROPERTY(EditAnywhere, Category = "Extraction")
	float ExtractionTime = 5.0f;

	/** Which controllers are currently inside the extraction zone. */
	UPROPERTY()
	TSet<TWeakObjectPtr<APlayerController>> ControllersInside;

	FTimerHandle ExtractionTimerHandle;

	bool bCountdownActive = false;
};