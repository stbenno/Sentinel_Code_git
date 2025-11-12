// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFW_InteractableInterface.generated.h"

UINTERFACE(Blueprintable)
class USFW_InteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTSENTINELLABS_API ISFW_InteractableInterface
{
	GENERATED_BODY()

public:
	// Must be BlueprintNativeEvent so implementors define *_Implementation
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
	FText GetPromptText() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
	void Interact(AController* InstigatorController);
};