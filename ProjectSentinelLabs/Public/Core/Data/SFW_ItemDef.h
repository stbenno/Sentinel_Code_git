// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h" 

#include <Core/Actors/SFW_EquippableBase.h>
#include "SFW_ItemDef.generated.h"

class UTexture2D;
class ASFW_EquippableBase;

/** Categories are lightweight; you can expand later (e.g., Tool, Consumable, Utility). */
UENUM(BlueprintType)
enum class ESFWItemKind : uint8
{
	Equippable  UMETA(DisplayName = "Equippable"),
	Consumable  UMETA(DisplayName = "Consumable"),
	Utility     UMETA(DisplayName = "Utility"),
};

UCLASS(BlueprintType)
class PROJECTSENTINELLABS_API USFW_ItemDef : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// PrimaryAssetId type = "ItemDef"
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("ItemDef"), GetFName());
	}

	/** Logical ID if you want a stable name decoupled from asset name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	UTexture2D* Icon = nullptr;

	/** Kind drives UI affordances (charges for consumables, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	ESFWItemKind Kind = ESFWItemKind::Equippable;

	/** If equippable, which slot it prefers when spawned/equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (EditCondition = "Kind==ESFWItemKind::Equippable"), Category = "Item|Equip")
	ESFWEquipSlot PreferredSlot = ESFWEquipSlot::None;

	/** Class to spawn at round start or on equip (for equippables). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Equip")
	TSubclassOf<ASFW_EquippableBase> EquippableClass;

	/** True if always provided (cannot be deselected). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	bool bIsBaseItem = false;

	/** True if selectable in the lobby menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	bool bIsOptional = true;

	/** Simple capacity/weight tuning. 0 disables weight cost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	float Weight = 0.f;

	/** Charges (for consumables). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"), Category = "Consumable")
	int32 DefaultCharges = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"), Category = "Consumable")
	int32 MaxCharges = 0;
};