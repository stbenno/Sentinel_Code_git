// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SFW_PlayerController.generated.h"

class ASFW_PlayerState;

/**
 * 
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_PlayerController : public APlayerController
{
	GENERATED_BODY()
	

public:
	/** UI calls this on the owning client to request a character/variant change. */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestSetCharacterAndVariant(FName InCharacterID, FName InVariantID);

	/** UI calls this on the owning client to request a ready state change. */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestSetReady(bool bNewReady);

protected:

	virtual void BeginPlay() override;

	/** Server RPC: apply character + variant on authoritative PlayerState. */
	UFUNCTION(Server, Reliable)
	void ServerSetCharacterAndVariant(FName InCharacterID, FName InVariantID);

	/** Server RPC: apply ready flag on authoritative PlayerState. */
	UFUNCTION(Server, Reliable)
	void ServerSetIsReady(bool bNewReady);

private:
	/** Convenience getter for our typed PlayerState. */
	ASFW_PlayerState* GetSFWPlayerState() const;

};
