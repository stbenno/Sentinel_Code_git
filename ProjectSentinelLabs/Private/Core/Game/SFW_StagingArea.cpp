// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/SFW_StagingArea.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"



// forward-declared in header:
// struct FSFWStageItemRequest;

ASFW_StagingArea::ASFW_StagingArea()
{
    bReplicates = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
}

void ASFW_StagingArea::StageItems(const TArray<FSFWStageItemRequest>& Requests)
{
    if (!HasAuthority())
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("StagingArea: StageItems called with %d entries"), Requests.Num());

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("StagingArea: No valid world to spawn into."));
        return;
    }

    if (Requests.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("StagingArea: No requests; nothing to spawn."));
        return;
    }

    if (ItemsPerRow <= 0)
    {
        ItemsPerRow = 4;
    }

    const FVector Origin = GetActorLocation();
    const FRotator Facing = GetActorRotation();
    const FVector Forward = Facing.Vector();      // X axis
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

    int32 SlotIndex = 0;

    for (const FSFWStageItemRequest& Req : Requests)
    {
        if (Req.TotalCount <= 0)
        {
            continue;
        }

        TSubclassOf<AActor>* ClassPtr = ItemClassMap.Find(Req.ItemId);
        if (!ClassPtr || !(*ClassPtr))
        {
            UE_LOG(LogTemp, Warning, TEXT("StagingArea: No class mapped for ItemId '%s' – skipping."),
                *Req.ItemId.ToString());
            continue;
        }

        TSubclassOf<AActor> ItemClass = *ClassPtr;

        for (int32 i = 0; i < Req.TotalCount; ++i)
        {
            const int32 Row = SlotIndex / ItemsPerRow;
            const int32 Col = SlotIndex % ItemsPerRow;

            const FVector BaseOffset =
                Forward * (Row * ItemSpacing) +
                Right * (Col * ItemSpacing);

            FVector SpawnLocation = Origin + BaseOffset;

            // Cheap downward trace to drop item onto floor
            FHitResult Hit;
            const FVector TraceStart = SpawnLocation + FVector(0.f, 0.f, 100.f);
            const FVector TraceEnd = SpawnLocation - FVector(0.f, 0.f, 500.f);
            FCollisionQueryParams Params(TEXT("StagingAreaTrace"), false, this);

            if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
            {
                SpawnLocation = Hit.Location;
            }

            const FTransform SpawnTM(Facing, SpawnLocation);

            AActor* Spawned = World->SpawnActor<AActor>(ItemClass, SpawnTM);
            if (Spawned)
            {
                UE_LOG(LogTemp, Log,
                    TEXT("StagingArea: Spawned %s for ItemId '%s' at %s"),
                    *Spawned->GetName(),
                    *Req.ItemId.ToString(),
                    *SpawnLocation.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("StagingArea: FAILED to spawn class %s for ItemId '%s'"),
                    *ItemClass->GetName(),
                    *Req.ItemId.ToString());
            }

            ++SlotIndex;
        }
    }
}

