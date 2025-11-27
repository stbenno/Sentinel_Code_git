// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Game/SFW_StagingTypes.h"
#include "SFW_StagingArea.generated.h"

struct FSFWStageItemRequest;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_StagingArea : public AActor
{
    GENERATED_BODY()

public:
    ASFW_StagingArea();

    /** Called by GameMode to spawn all staged items. Server only. */
    UFUNCTION(BlueprintCallable, Category = "Staging")
    void StageItems(const TArray<FSFWStageItemRequest>& Requests);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Staging")
    TObjectPtr<USceneComponent> Root;

    /** Map ItemId -> class to spawn (set up in BP_SFW_StagingArea). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging")
    TMap<FName, TSubclassOf<AActor>> ItemClassMap;

    /** How many items per row when laying them out. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Layout")
    int32 ItemsPerRow = 4;

    /** Spacing in cm between items. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staging|Layout")
    float ItemSpacing = 60.f;
};