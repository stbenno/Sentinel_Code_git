// Fill out your copyright notice in the Description page of Project Settings.

// SFW_Camera.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "SFW_Camera.generated.h"

class UStaticMeshComponent;
class USoundBase;
class UPrimitiveComponent;

/**
 * Handheld camera.
 * PrimaryUse() triggers a shutter for now.
 * Later you can add screenshot/evidence capture, film, etc.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_Camera : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_Camera();

	// Equippable
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;
	virtual void PrimaryUse() override;

	// Anim type
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;

protected:
	// Visual / physics body
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UStaticMeshComponent> CameraMesh;

	// Shutter SFX (played when a photo is taken)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Audio")
	TObjectPtr<USoundBase> ShutterSFX = nullptr;

	// Physics component when dropped
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

	// Server handles the "take photo" event
	UFUNCTION(Server, Reliable)
	void Server_FireShutter();

	// Cosmetics for all clients (sound, flash, etc.)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayShutterFX();

	// Hook for future: internal place to do gameplay side-effects
	void HandleShutterFired_Server();
};

