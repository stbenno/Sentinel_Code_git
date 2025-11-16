// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Game/SFW_GameInstance.h"
#include "GameFramework/PlayerState.h"

FString USFW_GameInstance::MakePlayerKey(const APlayerState* PS)
{
	if (!PS) return FString();
	if (PS->GetUniqueId().IsValid())
	{
		return PS->GetUniqueId()->ToString();
	}
	return PS->GetPlayerName();
}
