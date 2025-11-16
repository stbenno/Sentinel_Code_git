// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SFW_LoadoutRules.generated.h"

class USFW_ItemDef;

/**
 * Designer-driven constraints and lists for the lobby.
 * Make one asset (e.g., DA_LoadoutRules_Default) and assign Base / Optional pools.
 */
UCLASS(BlueprintType)
class PROJECTSENTINELLABS_API USFW_LoadoutRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// PrimaryAssetId type = "LoadoutRules"
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("LoadoutRules"), GetFName());
	}

	/** Max number of optional items a player may bring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	int32 MaxOptionalSlots = 3;

	/** Max total weight across optionals; 0 disables weight checks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	float MaxOptionalWeight = 0.f;

	/** Items always granted. You can flag them bIsBaseItem too, but this is the authoritative list. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pools")
	TArray<TSoftObjectPtr<USFW_ItemDef>> BaseItems;

	/** Items the lobby can display for selection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pools")
	TArray<TSoftObjectPtr<USFW_ItemDef>> OptionalItems;

	/** Basic validation helper (client-side precheck; final validation should happen on server). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rules")
	bool WouldSelectionBeValid(const TArray<USFW_ItemDef*>& Proposed, FString& OutFailureReason) const;
};