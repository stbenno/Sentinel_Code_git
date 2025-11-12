// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/SFW_PlayerController.h"

#include "Core/Game/SFW_PlayerState.h"

void ASFW_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Force focus back to the game when a match map loads
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;

	SetIgnoreLookInput(false);
	SetIgnoreMoveInput(false);
}

void ASFW_PlayerController::RequestSetCharacterAndVariant(FName InCharacterID, FName InVariantID)
{
	if (IsLocalController())
	{
		ServerSetCharacterAndVariant(InCharacterID, InVariantID);
	}
}

void ASFW_PlayerController::RequestSetReady(bool bNewReady)
{
	if (IsLocalController())
	{
		ServerSetIsReady(bNewReady);
	}
}



void ASFW_PlayerController::ServerSetCharacterAndVariant_Implementation(FName InCharacterID, FName InVariantID)
{
	if (ASFW_PlayerState* PS = GetSFWPlayerState())
	{
		// (Optional) validate lobby phase here before applying.
		PS->SetSelectedCharacterAndVariant(InCharacterID, InVariantID);
	}
}

void ASFW_PlayerController::ServerSetIsReady_Implementation(bool bNewReady)
{
	if (ASFW_PlayerState* PS = GetSFWPlayerState())
	{
		// (Optional) validate lobby phase here before applying.
		PS->SetIsReady(bNewReady);
	}
}

ASFW_PlayerState* ASFW_PlayerController::GetSFWPlayerState() const
{
	return GetPlayerState<ASFW_PlayerState>();
}