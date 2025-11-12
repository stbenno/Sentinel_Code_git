// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h"
#include "SFW_Thermometer.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

/**
 * Handheld thermometer.
 * PrimaryUse() toggles power.
 * Server updates CurrentTemperature based on environment/anomaly logic.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_Thermometer : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_Thermometer();

	// Lifecycle
	virtual void BeginPlay() override;
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;

	// Use
	virtual void PrimaryUse() override;

	// Anim type override
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;
	

	// Power
	UFUNCTION(BlueprintCallable, Category = "Thermometer")
	void SetActive(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Thermometer")
	bool IsActive() const { return bIsActive; }

	// Reading
	UFUNCTION(BlueprintPure, Category = "Thermometer")
	float GetCurrentTemperature() const { return CurrentTemperature; }

	// Server-side hook for world/anomaly systems
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Thermometer")
	void Server_SetTemperature(float NewTempCelsius);


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thermometer")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenMID;


protected:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thermometer")
	TObjectPtr<UStaticMeshComponent> ThermoMesh;

	// Physics
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

	

	// State
	UPROPERTY(ReplicatedUsing = OnRep_IsActive, BlueprintReadOnly, Category = "Thermometer")
	bool bIsActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTemperature, BlueprintReadOnly, Category = "Thermometer")
	float CurrentTemperature = 20.0f; // default room temp

	UFUNCTION()
	void OnRep_IsActive();

	UFUNCTION()
	void OnRep_CurrentTemperature();

	// Internal
	void ApplyActiveState();
	void ApplyTemperatureVisual();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
