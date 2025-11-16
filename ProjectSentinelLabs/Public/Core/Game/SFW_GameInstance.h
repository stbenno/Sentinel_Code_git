// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SFW_GameInstance.generated.h"

USTRUCT(BlueprintType)
struct FSFWPreMatchData
{
	GENERATED_BODY()

	// Character skin/variant chosen in lobby
	UPROPERTY(BlueprintReadWrite) FName CharacterID = NAME_None;
	UPROPERTY(BlueprintReadWrite) FName VariantID = FName(TEXT("Default"));

	// Selected OPTIONAL equipment IDs chosen in lobby (base items are granted in match GM)
	UPROPERTY(BlueprintReadWrite) TArray<FName> EquipmentIDs;
};

UCLASS()
class PROJECTSENTINELLABS_API USFW_GameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	/** Key: UniqueNetId (string) if valid, else PlayerName (for PIE/offline). */
	UPROPERTY() TMap<FString, FSFWPreMatchData> PreMatchCache;

	/** Store/overwrite a player's pre-match snapshot. */
	UFUNCTION(BlueprintCallable)
	void SetPreMatchData(const FString& Key, const FSFWPreMatchData& Data) { PreMatchCache.Add(Key, Data); }

	/** Read without removing. Returns false if not found. */
	UFUNCTION(BlueprintCallable)
	bool GetPreMatchData(const FString& Key, FSFWPreMatchData& Out) const
	{
		if (const FSFWPreMatchData* Found = PreMatchCache.Find(Key)) { Out = *Found; return true; }
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
	void ClearPreMatchData(const FString& Key) { PreMatchCache.Remove(Key); }

	/** Helper to derive a stable cache key from a PlayerState. */
	static FString MakePlayerKey(const class APlayerState* PS);
};
