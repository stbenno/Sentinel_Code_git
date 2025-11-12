// Fill out your copyright notice in the Description page of Project Settings.

// SFW_GeigerCounter.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "SFW_GeigerCounter.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class UPrimitiveComponent;

/**
 * Handheld Geiger counter.
 * PrimaryUse() toggles power on/off for now.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_GeigerCounter : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_GeigerCounter();

	// Equippable
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;
	virtual void PrimaryUse() override;

	// Anim type
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;

	// Power API
	UFUNCTION(BlueprintCallable, Category = "Geiger")
	void SetActive(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Geiger")
	bool IsActive() const { return bIsActive; }

protected:
	// Visual / physics body
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geiger")
	TObjectPtr<UStaticMeshComponent> GeigerMesh;

	// Optional looping tick / hiss while active
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geiger|Audio")
	TObjectPtr<UAudioComponent> LoopAudioComp;

	// One-shot power SFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geiger|Audio")
	TObjectPtr<USoundBase> PowerOnSFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Geiger|Audio")
	TObjectPtr<USoundBase> PowerOffSFX = nullptr;

	// Physics component when dropped
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

	// Replicated power state
	UPROPERTY(ReplicatedUsing = OnRep_IsActive, BlueprintReadOnly, Category = "Geiger")
	bool bIsActive = false;

	UFUNCTION()
	void OnRep_IsActive();

	// Server authority for power
	UFUNCTION(Server, Reliable)
	void Server_SetActive(bool bEnable);

	// Apply visuals/audio for current state
	void ApplyActiveState();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

