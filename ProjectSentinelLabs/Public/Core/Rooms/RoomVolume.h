// Fill out your copyright notice in the Description page of Project Settings.

// RoomVolume.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "RoomVolume.generated.h"

class APawn;
class APlayerState;
class ASFW_GameState;
class ASFW_PlayerState;
class USFW_AnomalyPropComponent;

UENUM(BlueprintType)
enum class ERoomType : uint8
{
    Base          UMETA(DisplayName = "Base"),
    RiftCandidate UMETA(DisplayName = "RiftCandidate"),
    Safe          UMETA(DisplayName = "Safe"),
    Hallway       UMETA(DisplayName = "Hallway")  // never Base or Rift
};

/**
 * Logical room volume. Tracks presence, safe rooms, rift flag.
 */
UCLASS()
class PROJECTSENTINELLABS_API ARoomVolume : public ATriggerBox
{
    GENERATED_BODY()
public:
    ARoomVolume();

    /** Logical room identifier (unique per room). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    FName RoomId = NAME_None;

    /** Higher wins when overlaps straddle two rooms. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    int32 Priority = 0;

    /** Mark this volume as a safe room. Mirrors legacy bIsSafeRoom flag. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
    bool bIsSafeRoom = false;

    /** Designer classification. Also drives bIsSafeRoom if Safe. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Type")
    ERoomType RoomType = ERoomType::Base;

    // ---- Kind helpers (Blueprint) ----
    UFUNCTION(BlueprintPure, Category = "Room|Type") bool IsHallway()      const { return RoomType == ERoomType::Hallway; }
    UFUNCTION(BlueprintPure, Category = "Room|Type") bool IsBaseKind()     const { return RoomType == ERoomType::Base; }
    UFUNCTION(BlueprintPure, Category = "Room|Type") bool IsSafeKind()     const { return RoomType == ERoomType::Safe; }
    UFUNCTION(BlueprintPure, Category = "Room|Type") bool IsRiftCandidate()const { return RoomType == ERoomType::RiftCandidate; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION() void HandleBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
    UFUNCTION() void HandleEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& E) override;
#endif

private:
    /** Per-actor debounce to avoid doorway spam. */
    UPROPERTY() TMap<TWeakObjectPtr<AActor>, float> LastEventTime;

        /** Minimum seconds between events for the same actor. */
    UPROPERTY(EditAnywhere, Category = "Room|Tuning")
    float DebounceSeconds = 0.15f;

    // Props currently overlapping this room
    UPROPERTY()
    TSet<TObjectPtr<USFW_AnomalyPropComponent>> AnomalyPropsInRoom;

    /** Per-player safe-room overlap count (supports nested safe volumes). */
    TMap<TWeakObjectPtr<APlayerState>, int32> SafeOverlapCount;

    bool IsPlayerPawn(AActor* Actor) const;
    APlayerState* GetPlayerStateFromActor(AActor* Actor) const;
    ASFW_PlayerState* GetSFWPlayerStateFromActor(AActor* Actor) const;
    bool ShouldProcess(AActor* Actor);

    void NotifyPresenceChanged(APlayerState* PS, bool bEnter) const;

    /** Adjust safe-room state for a player by +1 / -1 overlaps. */
    void UpdateSafeFlag(APawn* Pawn, int32 Delta);

    /** Mark this pawn's PlayerState as in the Rift if this volume == RiftRoom in GameState. */
    void UpdateRiftFlag(APawn* Pawn, bool bEnter);

    /** Cached pointer to GameState for quick checks */
    ASFW_GameState* GetSFWGameState() const;

public:
    const TSet<TObjectPtr<USFW_AnomalyPropComponent>>& GetAnomalyPropsInRoom() const
    {
        return AnomalyPropsInRoom;
    }
};
