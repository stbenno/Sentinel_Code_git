// Fill out your copyright notice in the Description page of Project Settings.

// RoomVolume.cpp

#include "Core/Rooms/RoomVolume.h"
#include "Core/Game/SFW_GameState.h"
#include "Core/Game/SFW_PlayerState.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Components/ShapeComponent.h"
#include "Core/Components/SFW_AnomalyPropComponent.h"

ARoomVolume::ARoomVolume()
{
    bNetLoadOnClient = true;

    // Overlap pawns only by default; adjust in BP if needed.
    GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetCollisionComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetCollisionComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    GetCollisionComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // for props
}

void ARoomVolume::BeginPlay()
{
    Super::BeginPlay();

    // Sync safe flag from enum. Hallway is never safe.
    if (RoomType == ERoomType::Safe)
    {
        bIsSafeRoom = true;
    }
    else if (RoomType == ERoomType::Hallway)
    {
        bIsSafeRoom = false;
    }

    // Warn if someone configured a hallway as Base/Rift in GameState.
    if (ASFW_GameState* GS = GetSFWGameState())
    {
        if (RoomType == ERoomType::Hallway)
        {
            if (GS->BaseRoom == this)
            {
                UE_LOG(LogTemp, Warning, TEXT("[RoomVolume] %s is Hallway but assigned as BaseRoom. This is not allowed by design."), *GetName());
            }
            if (GS->RiftRoom == this)
            {
                UE_LOG(LogTemp, Warning, TEXT("[RoomVolume] %s is Hallway but assigned as RiftRoom. This is not allowed by design."), *GetName());
            }
        }
    }

    OnActorBeginOverlap.AddDynamic(this, &ARoomVolume::HandleBeginOverlap);
    OnActorEndOverlap.AddDynamic(this, &ARoomVolume::HandleEndOverlap);
}

ASFW_GameState* ARoomVolume::GetSFWGameState() const
{
    return GetWorld() ? GetWorld()->GetGameState<ASFW_GameState>() : nullptr;
}

bool ARoomVolume::IsPlayerPawn(AActor* Actor) const
{
    const APawn* Pawn = Cast<APawn>(Actor);
    return Pawn && Pawn->IsPlayerControlled();
}

APlayerState* ARoomVolume::GetPlayerStateFromActor(AActor* Actor) const
{
    if (const APawn* Pawn = Cast<APawn>(Actor))
    {
        if (const AController* C = Pawn->GetController())
        {
            return C->PlayerState;
        }
    }
    return nullptr;
}

ASFW_PlayerState* ARoomVolume::GetSFWPlayerStateFromActor(AActor* Actor) const
{
    if (const APawn* Pawn = Cast<APawn>(Actor))
    {
        if (AController* C = Pawn->GetController())
        {
            return C->GetPlayerState<ASFW_PlayerState>();
        }
    }
    return nullptr;
}

bool ARoomVolume::ShouldProcess(AActor* Actor)
{
    const float Now = GetWorld()->TimeSeconds;
    float& LastTime = LastEventTime.FindOrAdd(Actor);
    if (Now - LastTime < DebounceSeconds) return false;
    LastTime = Now;
    return true;
}

void ARoomVolume::NotifyPresenceChanged(APlayerState* PS, bool bEnter) const
{
    UE_LOG(LogTemp, Log, TEXT("[RoomPresence] %s %s %s"),
        *GetNameSafe(PS),
        bEnter ? TEXT("ENTER") : TEXT("EXIT"),
        *RoomId.ToString());

    const ASFW_GameState* GS = GetSFWGameState();
    if (!GS) return;

    const bool bIsRift = (GS->RiftRoom == this);
    const bool bIsBase = (GS->BaseRoom == this);

    if (bIsRift)
    {
        UE_LOG(LogTemp, Log, TEXT("[RoomPresence] %s %s RIFT (%s)"),
            *GetNameSafe(PS),
            bEnter ? TEXT("ENTER") : TEXT("EXIT"),
            *RoomId.ToString());
    }
    if (bIsBase)
    {
        UE_LOG(LogTemp, Log, TEXT("[RoomPresence] %s %s BASE (%s)"),
            *GetNameSafe(PS),
            bEnter ? TEXT("ENTER") : TEXT("EXIT"),
            *RoomId.ToString());
    }
    if (RoomType == ERoomType::Hallway)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[RoomPresence] %s %s HALLWAY (%s)"),
            *GetNameSafe(PS),
            bEnter ? TEXT("ENTER") : TEXT("EXIT"),
            *RoomId.ToString());
    }
}

void ARoomVolume::UpdateSafeFlag(APawn* Pawn, int32 Delta)
{
    if (!Pawn) return;

    ASFW_PlayerState* PS = Pawn->GetPlayerState<ASFW_PlayerState>();
    if (!PS) return;

    int32& Count = SafeOverlapCount.FindOrAdd(PS);
    Count = FMath::Max(0, Count + Delta);

    if (PS->HasAuthority())
    {
        PS->SetInSafeRoom(Count > 0);
    }
}

void ARoomVolume::UpdateRiftFlag(APawn* Pawn, bool bEnter)
{
    if (!Pawn) return;

    ASFW_GameState* GS = GetSFWGameState();
    if (!GS) return;

    // Only the server should mark flags on PlayerState
    ASFW_PlayerState* PS = Pawn->GetPlayerState<ASFW_PlayerState>();
    if (!PS || !PS->HasAuthority()) return;

    const bool bThisIsRift = (GS->RiftRoom == this);

    if (!bThisIsRift)
    {
        // Only changes when this volume IS the active RiftRoom
        return;
    }

    PS->SetInRiftRoom(bEnter);
}

void ARoomVolume::HandleBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    if (!HasAuthority() || !OtherActor || !ShouldProcess(OtherActor))
    {
        return;
    }

    // 1) Player handling (unchanged)
    if (IsPlayerPawn(OtherActor))
    {
        APawn* Pawn = Cast<APawn>(OtherActor);
        APlayerState* GenericPS = GetPlayerStateFromActor(OtherActor);
        if (GenericPS)
        {
            NotifyPresenceChanged(GenericPS, /*bEnter*/ true);
        }

        if (bIsSafeRoom && RoomType != ERoomType::Hallway)
        {
            UpdateSafeFlag(Pawn, +1);
        }

        UpdateRiftFlag(Pawn, /*bEnter*/ true);
    }

    // 2) Prop handling
    if (USFW_AnomalyPropComponent* PropComp =
        OtherActor->FindComponentByClass<USFW_AnomalyPropComponent>())
    {
        AnomalyPropsInRoom.Add(PropComp);

        UE_LOG(LogTemp, Log,
            TEXT("[RoomVolume] %s: prop ENTER %s"),
            *RoomId.ToString(),
            *GetNameSafe(OtherActor));
    }
}

void ARoomVolume::HandleEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    if (!HasAuthority() || !OtherActor || !ShouldProcess(OtherActor))
    {
        return;
    }

    // 1) Player handling (unchanged)
    if (IsPlayerPawn(OtherActor))
    {
        APawn* Pawn = Cast<APawn>(OtherActor);
        APlayerState* GenericPS = GetPlayerStateFromActor(OtherActor);
        if (GenericPS)
        {
            NotifyPresenceChanged(GenericPS, /*bEnter*/ false);
        }

        if (bIsSafeRoom && RoomType != ERoomType::Hallway)
        {
            UpdateSafeFlag(Pawn, -1);
        }

        UpdateRiftFlag(Pawn, /*bEnter*/ false);
    }

    // 2) Prop handling
    if (USFW_AnomalyPropComponent* PropComp =
        OtherActor->FindComponentByClass<USFW_AnomalyPropComponent>())
    {
        AnomalyPropsInRoom.Remove(PropComp);

        UE_LOG(LogTemp, Log,
            TEXT("[RoomVolume] %s: prop EXIT %s"),
            *RoomId.ToString(),
            *GetNameSafe(OtherActor));
    }
}


#if WITH_EDITOR
void ARoomVolume::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);

    // Enforce invariants in editor: Hallway can’t be safe.
    if (RoomType == ERoomType::Hallway && bIsSafeRoom)
    {
        bIsSafeRoom = false;
        UE_LOG(LogTemp, Warning, TEXT("[RoomVolume] %s: Hallway cannot be marked Safe. Cleared bIsSafeRoom."), *GetName());
    }
}
#endif
