// Fill out your copyright notice in the Description page of Project Settings.

// SFW_SoundSensor.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "SFW_SoundSensor.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class UPrimitiveComponent;

/**
 * Handheld sound sensor.
 * PrimaryUse() toggles it on/off for now.
 * Later you can add placement / world-listening behavior.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_SoundSensor : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_SoundSensor();

	// Equippable
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;
	virtual void PrimaryUse() override;

	// Anim type
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;

	// Power API
	UFUNCTION(BlueprintCallable, Category = "SoundSensor")
	void SetActive(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "SoundSensor")
	bool IsActive() const { return bIsActive; }

protected:
	// Visual / physics body
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SoundSensor")
	TObjectPtr<UStaticMeshComponent> SensorMesh;

	// Optional loop / processing hum
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SoundSensor|Audio")
	TObjectPtr<UAudioComponent> LoopAudioComp;

	// One-shot power SFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoundSensor|Audio")
	TObjectPtr<USoundBase> PowerOnSFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SoundSensor|Audio")
	TObjectPtr<USoundBase> PowerOffSFX = nullptr;

	// Physics component when dropped
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

	// Replicated power state
	UPROPERTY(ReplicatedUsing = OnRep_IsActive, BlueprintReadOnly, Category = "SoundSensor")
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

