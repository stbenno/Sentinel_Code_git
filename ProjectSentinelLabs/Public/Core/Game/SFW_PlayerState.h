// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SFW_PlayerState.generated.h"

// ----- Delegates -----
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterIDChanged, FName, NewCharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVariantIDChanged, FName, NewVariantID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReadyChanged, bool, bNewIsReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerNameChanged, const FString&, NewName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHostFlagChanged, bool, bNewIsHost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSanityChanged, float, NewSanity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInRiftChanged, bool, bNowInRift);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBlackoutChanged, bool, bNowBlackedOut);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSafeRoomChanged);

// New: fires when the optional loadout array changes (clients + server)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOptionalsChanged);

UENUM(BlueprintType)
enum class ESanityTier : uint8
{
	T1 UMETA(DisplayName = "Tier 1"),
	T2 UMETA(DisplayName = "Tier 2"),
	T3 UMETA(DisplayName = "Tier 3")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSanityTierChanged, ESanityTier, NewTier);

class USFW_AgentCatalog;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_PlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	ASFW_PlayerState();

	// ---------- Lobby / appearance ----------
	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterID, BlueprintReadOnly, Category = "Appearance")
	FName SelectedCharacterID = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedVariantID, BlueprintReadOnly, Category = "Appearance")
	FName SelectedVariantID = FName(TEXT("Default"));

	UPROPERTY(ReplicatedUsing = OnRep_IsReady, BlueprintReadOnly, Category = "Lobby")
	bool bIsReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsHost, BlueprintReadOnly, Category = "Lobby")
	bool bIsHost = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<USFW_AgentCatalog> AgentCatalog = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_CharacterIndex, BlueprintReadOnly, Category = "Appearance")
	int32 CharacterIndex = 0;

	// ---------- Optional equipment selection (Lobby) ----------
	/** Max optional slots the player is allowed to take (server may update this from LoadoutRules). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Loadout")
	int32 OptionalSlotsLimit = 3;

	/** Selected optional item IDs (deduped; clamped to OptionalSlotsLimit). */
	UPROPERTY(ReplicatedUsing = OnRep_SelectedOptionals, BlueprintReadOnly, Category = "Loadout")
	TArray<FName> SelectedOptionalIDs;

	// Read access for widgets/blueprints
	UFUNCTION(BlueprintPure, Category = "Loadout")
	const TArray<FName>& GetSelectedOptionalIDs() const { return SelectedOptionalIDs; }

	// Server-side set/modify from lobby UI
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Loadout")
	void ServerSetSelectedOptionals(const TArray<FName>& NewIDs);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Loadout")
	void ServerAddOptional(FName OptionalID);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Loadout")
	void ServerRemoveOptional(FName OptionalID);

	/** Server can update the slot limit (e.g., from data asset rules). Will clamp current selection. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Loadout")
	void ServerSetOptionalSlotsLimit(int32 NewLimit);

	// ---------- Anomaly / sanity ----------
	UPROPERTY(ReplicatedUsing = OnRep_Sanity, BlueprintReadOnly, Category = "Anomaly")
	float Sanity = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_SanityTier, BlueprintReadOnly, Category = "Anomaly")
	ESanityTier SanityTier = ESanityTier::T1;

	// Tunables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Sanity")
	float SanityTier1Min = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Sanity")
	float SanityTier2Min = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Sanity")
	float SanityHysteresis = 5.f;

	// Passive drift settings (server-driven)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Sanity")
	float BaseDrainPerSec = 0.2f;          // default drain when not in safe room

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Sanity")
	float SafeRoomRecoveryPerSec = 0.3f;   // passive recovery when in safe room

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Sanity")
	float RecoveryCeilPct = 0.8f;          // passive recovery cap = 80% of max

	UPROPERTY(ReplicatedUsing = OnRep_InRiftRoom, BlueprintReadOnly, Category = "Anomaly")
	bool bInRiftRoom = false;

	UPROPERTY(ReplicatedUsing = OnRep_Blackout, BlueprintReadOnly, Category = "Anomaly")
	bool bIsBlackedOut = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Anomaly")
	float BlackoutEndTime = 0.f;

	// Safe room flag to enable passive recovery
	UPROPERTY(ReplicatedUsing = OnRep_SafeRoom, BlueprintReadOnly, Category = "Anomaly|Sanity")
	bool bInSafeRoom = false;

	// ---------- Events ----------
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnCharacterIDChanged OnSelectedCharacterIDChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnVariantIDChanged OnSelectedVariantIDChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnReadyChanged OnReadyChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnPlayerNameChanged OnPlayerNameChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnHostFlagChanged OnHostFlagChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnSanityChanged OnSanityChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnSanityTierChanged OnSanityTierChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnInRiftChanged OnInRiftChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnBlackoutChanged OnBlackoutChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnSafeRoomChanged OnSafeRoomChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnOptionalsChanged OnOptionalsChanged;

	// ---------- Lobby API (server only) ----------
	UFUNCTION(BlueprintCallable, Category = "Lobby") void SetSelectedCharacterAndVariant(const FName& InCharacterID, const FName& InVariantID);
	UFUNCTION(BlueprintCallable, Category = "Lobby") void SetIsReady(bool bNewReady);
	UFUNCTION(BlueprintCallable, Category = "Lobby") void ResetForLobby();
	UFUNCTION(BlueprintPure, Category = "Lobby") bool GetIsHost() const { return bIsHost; }
	void ServerSetIsHost(bool bNewIsHost);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Appearance") void ServerSetCharacterIndex(int32 NewIndex);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Appearance") void ServerCycleCharacter(int32 Direction);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Appearance") void ServerSetCharacterByID(FName InCharacterID);

	// ---------- Anomaly API (server only) ----------
	UFUNCTION(BlueprintCallable, Category = "Anomaly") void ApplySanityDelta(float Delta);
	UFUNCTION(BlueprintCallable, Category = "Anomaly") void SetSanity(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Anomaly") void SetInRiftRoom(bool bIn);
	UFUNCTION(BlueprintCallable, Category = "Anomaly") void StartBlackout(float DurationSeconds);
	UFUNCTION(BlueprintCallable, Category = "Anomaly") void ClearBlackout();
	UFUNCTION(BlueprintCallable, Category = "Anomaly") void SetInSafeRoom(bool bIn);

	UFUNCTION(BlueprintPure, Category = "Anomaly") bool IsStandingAliveForExtraction() const { return !bIsBlackedOut; }
	UFUNCTION(BlueprintPure, Category = "Anomaly") ESanityTier GetSanityTier() const { return SanityTier; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// ----- RepNotifies -----
	UFUNCTION() void OnRep_SelectedCharacterID();
	UFUNCTION() void OnRep_SelectedVariantID();
	UFUNCTION() void OnRep_IsReady();
	UFUNCTION() void OnRep_IsHost();
	UFUNCTION() void OnRep_CharacterIndex();
	UFUNCTION() void OnRep_Sanity();
	UFUNCTION() void OnRep_SanityTier();
	UFUNCTION() void OnRep_InRiftRoom();
	UFUNCTION() void OnRep_Blackout();
	UFUNCTION() void OnRep_SafeRoom();
	UFUNCTION() void OnRep_SelectedOptionals();

	virtual void OnRep_PlayerName() override;

	// ----- Helpers -----
	int32 GetAgentCount() const;
	void NormalizeIndex();
	void ApplyIndexToSelectedID();
	int32 FindIndexByAgentID(FName InCharacterID) const;

	// Sanity tier recompute (server sets + replicates)
	void RecomputeAndApplySanityTier();

	// Passive drift tick (server)
	void SanityTick();

	// --- Loadout helpers (server only) ---
	void SanitizeOptionals(const TArray<FName>& InIDs, TArray<FName>& OutIDs) const;
	void ClampToSlotLimit(TArray<FName>& InOutIDs) const;
	void DedupAndStripNones(TArray<FName>& InOutIDs) const;

	UPROPERTY(EditAnywhere, Category = "Sanity")
	TSubclassOf<AActor> ShadeClass;

	UPROPERTY(EditAnywhere, Category = "Sanity")
	float ShadeRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Sanity")
	float ShadeDrainMultiplier = 2.5f;

private:
	FTimerHandle SanityTickHandle;
};
