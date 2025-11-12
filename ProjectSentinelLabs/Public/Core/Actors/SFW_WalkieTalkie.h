// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "SFW_WalkieTalkie.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

/**
 * Handheld radio. Owning this in inventory allows long-range comms.
 * Holding it (PrimaryUse) will later key radio transmit.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_WalkieTalkie : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_WalkieTalkie();

	// This item grants access to radio comms
	virtual bool GrantsRadioComms_Implementation() const override { return true; }

	// Called when player "uses" the walkie (press IA_Use while it's active hand item)
	virtual void PrimaryUse() override;

	// Anim type override
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;

protected:
	// Visible / physical body
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Walkie")
	TObjectPtr<UStaticMeshComponent> WalkieMesh;

	// Which component should get physics when dropped.
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

	// Equip / unequip hooks to manage physics and visibility
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;
};


