// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SFW_GameState.generated.h"

class AActor;

/** Which anomaly archetype is running this match */
UENUM(BlueprintType)
enum class EAnomalyClass : uint8
{
	Binder     UMETA(DisplayName = "Binder"),
	Watcher    UMETA(DisplayName = "Watcher"),
	Chill      UMETA(DisplayName = "Chill"),
	Parasite   UMETA(DisplayName = "Parasite"),
	Splitter   UMETA(DisplayName = "Splitter")
};

/* Game state - To complete the match loop*/
UENUM(BlueprintType)
enum class ESFWRoundResult : uint8
{
	None    UMETA(DisplayName = "None"),
	Success UMETA(DisplayName = "Success"),
	Fail    UMETA(DisplayName = "Fail")
};

UCLASS()
class PROJECTSENTINELLABS_API ASFW_GameState : public AGameState
{
	GENERATED_BODY()

public:
	ASFW_GameState();

	// ------------------------
	// Round
	// ------------------------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	bool bRoundActive;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	int32 RoundSeed;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	float RoundStartTime;

	UPROPERTY(Replicated, BlueprintReadOnly) bool bRoundInProgress = false;
	UPROPERTY(Replicated, BlueprintReadOnly) float RoundEndTime = -1.f;

	UFUNCTION(BlueprintCallable) void BeginRound(float Now, int32 Seed);
	UFUNCTION(BlueprintCallable) void EndRound(float Now);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	bool bExtractionCountdownActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	float ExtractionEndTime = 0.f;   // server WorldTimeSeconds when it will complete
	



	// ------------------------
	// Global pacing / anomaly info
	// ------------------------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Anomaly")
	float AnomalyAggression;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Anomaly")
	EAnomalyClass ActiveClass;

	// NEW
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	ESFWRoundResult RoundResult = ESFWRoundResult::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	float RoundDuration = 0.f;


	// ------------------------
	// Rooms
	// ------------------------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rooms")
	AActor* BaseRoom;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rooms")
	AActor* RiftRoom;

	// ------------------------
	// Evidence window
	// Replicates to clients. Devices read this.
	// ------------------------
	UPROPERTY(ReplicatedUsing = OnRep_EvidenceWindow, BlueprintReadOnly, Category = "Evidence")
	bool bEvidenceWindowActive;

	// When this evidence window started (server time seconds)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Evidence")
	float EvidenceWindowStartTime;

	// How long it should last
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Evidence")
	float EvidenceWindowDurationSec;

	// 0 = none. 1..9 = EMF / Thermo / UV etc
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Evidence")
	int32 CurrentEvidenceType;

	UFUNCTION()
	void OnRep_EvidenceWindow();

	// Server starts a new evidence window
	UFUNCTION(Server, Reliable)
	void Server_StartEvidenceWindow(int32 EvidenceType, float DurationSec);

	// Server ends the active evidence window if any
	UFUNCTION(Server, Reliable)
	void Server_EndEvidenceWindow();

	// Server tells player gear to react to the current clue
	UFUNCTION(Server, Reliable)
	void Server_TriggerEvidence(int32 EvidenceType);

	// ------------------------
	// Binder door scare budget
	// This gates how many "slam in face" scares can run per match.
	// ------------------------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Binder")
	int32 BinderDoorScareBudget;

	// Read only helper
	UFUNCTION(BlueprintCallable, Category = "Binder")
	bool CanFireBinderDoorScare() const { return BinderDoorScareBudget > 0; }

	// Consume one charge (server only)
	UFUNCTION(BlueprintCallable, Category = "Binder")
	void ConsumeBinderDoorScare();

	// ------------------------
	// Radio comms
	// ------------------------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Radio")
	bool bRadioJammed;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Radio")
	float RadioIntegrity;

	UFUNCTION(BlueprintPure, Category = "Radio")
	bool IsRadioJammed() const { return bRadioJammed; }

	UFUNCTION(BlueprintPure, Category = "Radio")
	float GetRadioIntegrity() const { return RadioIntegrity; }

	UFUNCTION(BlueprintCallable, Category = "Radio")
	void Server_SetRadioJammed(bool bJammed);

	UFUNCTION(BlueprintCallable, Category = "Radio")
	void Server_SetRadioIntegrity(float NewIntegrity);

	// ------------------------
	// Replication
	// ------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
