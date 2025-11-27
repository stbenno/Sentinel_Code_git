// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Game/SFW_GameInstance.h"
#include "GameFramework/PlayerState.h"

FString USFW_GameInstance::MakePlayerKey(const APlayerState* PS)
{
    if (!PS)
    {
        return FString();
    }

    // Prefer network ID when available
    const FUniqueNetIdRepl& NetIdRepl = PS->GetUniqueId();
    if (NetIdRepl.IsValid())
    {
        return NetIdRepl->ToString();
    }

    // Fallback for PIE / offline
    return PS->GetPlayerName();
}