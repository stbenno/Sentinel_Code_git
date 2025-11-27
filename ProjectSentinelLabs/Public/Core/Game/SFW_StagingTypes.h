#pragma once


// SFW_StagingTypes.h

#pragma once

#include "CoreMinimal.h"
#include "SFW_StagingTypes.generated.h"

/**
 * High-level staging category for pre-match items.
 * Used to choose which spawn point array to use in ASFW_StagingArea.
 */
UENUM(BlueprintType)
enum class ESFWStageCategory : uint8
{
    Light      UMETA(DisplayName = "Light"),
    Tool       UMETA(DisplayName = "Tool"),
    Consumable UMETA(DisplayName = "Consumable"),
    Etc        UMETA(DisplayName = "Etc")
};

/**
 * Aggregated staging request for a single item type.
 * Built in the MatchGameMode from USFW_GameInstance::PreMatchCache.
 */
USTRUCT(BlueprintType)
struct FSFWStageItemRequest
{
    GENERATED_BODY()

    /** Item identifier (matches your item definition ID, e.g. EquipmentID). */
    UPROPERTY(BlueprintReadWrite, Category = "Staging")
    FName ItemId = NAME_None;

    /** Total number of units of this item to be staged for the team. */
    UPROPERTY(BlueprintReadWrite, Category = "Staging")
    int32 TotalCount = 0;
};
