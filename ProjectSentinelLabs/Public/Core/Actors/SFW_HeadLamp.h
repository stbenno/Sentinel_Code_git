// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "SFW_HeadLamp.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;
class USoundBase;
class UTextureLightProfile;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_HeadLamp : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_HeadLamp();

	// Equippable overrides
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;

	// Interact: special case, goes into headlamp slot (not inventory)
	virtual void Interact_Implementation(AController* InstigatorController) override;

protected:
	// Tell base which component should simulate when dropped
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

public:
	// Toggle API called by EquipmentManager::Server_ToggleHeadLamp
	UFUNCTION(BlueprintCallable, Category = "HeadLamp")
	void ToggleLamp();

	UFUNCTION(BlueprintCallable, Category = "HeadLamp")
	void SetLampEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "HeadLamp")
	bool IsLampEnabled() const { return bLampEnabled; }

protected:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeadLamp")
	TObjectPtr<UStaticMeshComponent> HeadlampMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeadLamp")
	TObjectPtr<USpotLightComponent> Lamp;

	// Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_LampEnabled, BlueprintReadOnly, Category = "HeadLamp")
	bool bLampEnabled = false;

	UFUNCTION()
	void OnRep_LampEnabled();

	// Socket and offsets
	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Attach")
	FName HeadSocketName = TEXT("head_LampSocket");

	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Attach")
	FVector AttachOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Attach")
	FRotator AttachRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Attach")
	FVector AttachScale = FVector(0.3f, 0.3f, 0.3f);

	// Light tuning
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeadLamp|Light")
	float OnIntensity = 40000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeadLamp|Light")
	float AttenuationRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeadLamp|Light")
	float InnerCone = 24.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeadLamp|Light")
	float OuterCone = 36.f;

	// SFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeadLamp|Audio")
	TObjectPtr<USoundBase> ToggleOnSFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeadLamp|Audio")
	TObjectPtr<USoundBase> ToggleOffSFX = nullptr;

	// IES profile
	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Photometric")
	TObjectPtr<UTextureLightProfile> IESProfile = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Photometric")
	bool bUseIESBrightness = true;

	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Photometric")
	float IESBrightnessScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HeadLamp|Photometric")
	ELightUnits IntensityUnits = ELightUnits::Lumens;

	// RPCs
	UFUNCTION(Server, Reliable)
	void Server_SetLampEnabled(bool bEnabled);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayToggleSFX(bool bNewEnabled);

	// ASFW_EquippableBase will use this for AttachToCharacter
	virtual FName GetAttachSocketName() const override { return HeadSocketName; }

	// Local apply
	void ApplyLightState();

	// Rep boilerplate
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

