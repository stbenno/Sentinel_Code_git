// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SFW_LoadoutSubsystem.generated.h"

class USFW_ItemDef;
class USFW_LoadoutRules;

/** Simple item + optional per-item charges (for consumables). */
USTRUCT(BlueprintType)
struct FSFW_SelectedOptional
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USFW_ItemDef> Item;

	/** If the item is consumable, how many charges to bring (<= MaxCharges). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Charges = 0;
};

/**
 * Client-only selection cache used by the lobby UI.
 * In Step 2 we'll add a replicated handoff (PlayerController->Server) and server validation.
 */
UCLASS()
class PROJECTSENTINELLABS_API USFW_LoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Assign the rules asset the lobby should use (e.g., on lobby map load). */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void SetActiveRules(USFW_LoadoutRules* InRules) { ActiveRules = InRules; }

	UFUNCTION(BlueprintPure, Category = "Loadout")
	USFW_LoadoutRules* GetActiveRules() const { return ActiveRules; }

	/** Base items are read from Rules; exposed here for UI convenience. */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	void GetBaseItems(TArray<USFW_ItemDef*>& Out) const;

	/** Get/Set current optional selection. */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	void GetSelectedOptionals(TArray<FSFW_SelectedOptional>& Out) const { Out = SelectedOptionals; }

	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool TrySetSelectedOptionals(const TArray<FSFW_SelectedOptional>& InNewSelection, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void ClearSelection();

private:
	/** Not saved across sessions for now; just per-run. */
	UPROPERTY()
	TObjectPtr<USFW_LoadoutRules> ActiveRules = nullptr;

	UPROPERTY()
	TArray<FSFW_SelectedOptional> SelectedOptionals;
};
