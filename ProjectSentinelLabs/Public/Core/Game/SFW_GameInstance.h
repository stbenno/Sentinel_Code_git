// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SFW_GameInstance.generated.h"

USTRUCT(BlueprintType)
struct FSFWPreMatchData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FName CharacterID = NAME_None;
	UPROPERTY(BlueprintReadWrite) FName VariantID = FName(TEXT("Default"));
	UPROPERTY(BlueprintReadWrite) TArray<FName> EquipmentIDs;
};

UCLASS()
class PROJECTSENTINELLABS_API USFW_GameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	// Key: UniqueNetId (string) if valid, else PlayerName (for PIE/offline)
	UPROPERTY() TMap<FString, FSFWPreMatchData> PreMatchCache;

	UFUNCTION(BlueprintCallable) void SetPreMatchData(const FString& Key, const FSFWPreMatchData& Data) { PreMatchCache.Add(Key, Data); }
	UFUNCTION(BlueprintCallable) bool GetPreMatchData(const FString& Key, FSFWPreMatchData& Out) const
	{
		if (const FSFWPreMatchData* Found = PreMatchCache.Find(Key)) { Out = *Found; return true; }
		return false;
	}

	static FString MakePlayerKey(const class APlayerState* PS); // helper
};