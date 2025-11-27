// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/Game/SFW_GameState.h"
#include "SFW_GameInstance.generated.h"

// Forward declare the round result enum defined in SFW_GameState.h
// (Do NOT re-UENUM it here.)
enum class ESFWRoundResult : uint8;

USTRUCT(BlueprintType)
struct FSFWPreMatchData
{
	GENERATED_BODY()

	// Character skin/variant chosen in lobby
	UPROPERTY(BlueprintReadWrite)
	FName CharacterID = NAME_None;

	UPROPERTY(BlueprintReadWrite)
	FName VariantID = FName(TEXT("Default"));

	// Selected OPTIONAL equipment IDs chosen in lobby (base items are granted in match GM)
	UPROPERTY(BlueprintReadWrite)
	TArray<FName> EquipmentIDs;
};

USTRUCT(BlueprintType)
struct FSFWLastRoundSummary
{
	GENERATED_BODY()

	// Overall result: None / Success / Fail
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	ESFWRoundResult Result = ESFWRoundResult::None;

	// Total duration in seconds (server-side)
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	float DurationSeconds = 0.f;

	// Map identifier or map name (you can decide what to feed into this)
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FName MapId = NAME_None;

	// Server world time when the round ended (for ordering / debugging)
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	float EndTimeSeconds = 0.f;
};

UCLASS()
class PROJECTSENTINELLABS_API USFW_GameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	/** Key: UniqueNetId (string) if valid, else PlayerName (for PIE/offline). */
	UPROPERTY()
	TMap<FString, FSFWPreMatchData> PreMatchCache;

	/** Store/overwrite a player's pre-match snapshot. */
	UFUNCTION(BlueprintCallable)
	void SetPreMatchData(const FString& Key, const FSFWPreMatchData& Data)
	{
		PreMatchCache.Add(Key, Data);
	}

	/** Read without removing. Returns false if not found. */
	UFUNCTION(BlueprintCallable)
	bool GetPreMatchData(const FString& Key, FSFWPreMatchData& Out) const
	{
		if (const FSFWPreMatchData* Found = PreMatchCache.Find(Key))
		{
			Out = *Found;
			return true;
		}
		return false;
	}

	/** Read and remove. Returns false if not found. */
	UFUNCTION(BlueprintCallable)
	bool ConsumePreMatchData(const FString& Key, FSFWPreMatchData& Out)
	{
		if (GetPreMatchData(Key, Out))
		{
			PreMatchCache.Remove(Key);
			return true;
		}
		return false;
	}

	/** Remove a player's snapshot (if present). */
	UFUNCTION(BlueprintCallable)
	void ClearPreMatchData(const FString& Key)
	{
		PreMatchCache.Remove(Key);
	}

	/** Helper to derive a stable cache key from a PlayerState. */
	static FString MakePlayerKey(const class APlayerState* PS);

	// ------------------------
	// Last round summary
	// ------------------------

	/** Snapshot of the most recent round, persisted across travel back to lobby. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FSFWLastRoundSummary LastRoundSummary;

	/** Overwrite the last round summary (called by GameMode on round end). */
	UFUNCTION(BlueprintCallable, Category = "Round")
	void SetLastRoundSummary(const FSFWLastRoundSummary& InSummary)
	{
		LastRoundSummary = InSummary;
	}

	/** Read last round summary (for lobby UI / post-round screen). */
	UFUNCTION(BlueprintCallable, Category = "Round")
	FSFWLastRoundSummary GetLastRoundSummary() const
	{
		return LastRoundSummary;
	}

	/** Optional: clear it explicitly if you ever need to. */
	UFUNCTION(BlueprintCallable, Category = "Round")
	void ClearLastRoundSummary()
	{
		LastRoundSummary = FSFWLastRoundSummary();
	}
};
