// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Actors/SFW_Thermometer.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

ASFW_Thermometer::ASFW_Thermometer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	bIsActive = false;
	CurrentTemperature = 20.0f;

	EquipSlot = ESFWEquipSlot::Hand_Thermo;
}

void ASFW_Thermometer::BeginPlay()
{
	Super::BeginPlay();

	if (ScreenMesh)
	{
		ScreenMID = ScreenMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	ApplyActiveState();
	ApplyTemperatureVisual();
}

void ASFW_Thermometer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_Thermometer, bIsActive);
	DOREPLIFETIME(ASFW_Thermometer, CurrentTemperature);
}

UPrimitiveComponent* ASFW_Thermometer::GetPhysicsComponent() const
{
	return ThermoMesh ? ThermoMesh : Super::GetPhysicsComponent();
}

void ASFW_Thermometer::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);

	if (ThermoMesh)
	{
		ThermoMesh->SetSimulatePhysics(false);
		ThermoMesh->SetEnableGravity(false);
		ThermoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ThermoMesh->SetVisibility(true, true);
	}

	ApplyActiveState();
	ApplyTemperatureVisual();
}

void ASFW_Thermometer::OnUnequipped()
{
	if (ThermoMesh)
	{
		ThermoMesh->SetSimulatePhysics(false);
		ThermoMesh->SetEnableGravity(false);
		ThermoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Super::OnUnequipped();
}

void ASFW_Thermometer::PrimaryUse()
{
	// Simple toggle for now
	SetActive(!bIsActive);
}

EHeldItemType ASFW_Thermometer::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::Thermometer;
}

void ASFW_Thermometer::SetActive(bool bEnable)
{
	if (!HasAuthority())
	{
		// You can add a Server_SetActive RPC later if you want strict server control
		bIsActive = bEnable;
		ApplyActiveState();
		return;
	}

	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_Thermometer::Server_SetTemperature_Implementation(float NewTempCelsius)
{
	CurrentTemperature = NewTempCelsius;
	ApplyTemperatureVisual();
}

void ASFW_Thermometer::OnRep_IsActive()
{
	ApplyActiveState();
}

void ASFW_Thermometer::OnRep_CurrentTemperature()
{
	ApplyTemperatureVisual();
}

void ASFW_Thermometer::ApplyActiveState()
{
	// Placeholder. Later you can drive a screen emissive / beep, etc.
	// For now this just exists so you can hook materials or audio in BP.
}

void ASFW_Thermometer::ApplyTemperatureVisual()
{
	// Placeholder hook. In BP you can read CurrentTemperature and update
	// a text render or emissive scale on the display.
}
