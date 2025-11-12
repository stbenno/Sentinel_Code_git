// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Actors/Interface/SFW_InteractableInterface.h"


#include "SFW_WorldPickup.generated.h"


class UStaticMeshComponent;
class UBoxComponent;
class ASFW_HeadLamp;
class ASFW_EquippableBase;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_WorldPickup : public AActor, public ISFW_InteractableInterface
{
	GENERATED_BODY()

public:
	ASFW_WorldPickup();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UBoxComponent> UseBox;

	/** Set this to your BP_SFW_HeadLamp (child of ASFW_HeadLamp) in the editor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	TSubclassOf<ASFW_EquippableBase> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FText ItemDisplayName;

public:
	// --- Interactable Interface ---
	virtual FText GetPromptText_Implementation() const override;
	virtual void Interact_Implementation(AController* InstigatorController) override;

protected:
	UFUNCTION(Server, Reliable)
	void Server_GiveTo(AController* InstigatorController);
};