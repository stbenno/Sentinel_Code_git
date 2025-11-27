// Fill out your copyright notice in the Description page of Project Settings.

// SFW_Flashlight.cpp

#include "Core/Actors/SFW_Flashlight.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Components/PrimitiveComponent.h"

ASFW_Flashlight::ASFW_Flashlight()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	EquipSlot = ESFWEquipSlot::Hand_Light;

	// Light
	Spot = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spot"));
	Spot->SetupAttachment(FlashlightMesh);
	Spot->Mobility = EComponentMobility::Movable;
	Spot->IntensityUnits = IntensityUnits;
	Spot->bUseInverseSquaredFalloff = false;
	Spot->AttenuationRadius = AttenuationRadius;
	Spot->Intensity = 0.f;                 // start OFF
	Spot->InnerConeAngle = InnerCone;
	Spot->OuterConeAngle = OuterCone;
	Spot->SetVisibility(false, true);

	bIsOn = false;
}

void ASFW_Flashlight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASFW_Flashlight, bIsOn);
}

UPrimitiveComponent* ASFW_Flashlight::GetPhysicsComponent() const
{
	// Use the static mesh as the physics body for drop / pickup
	return FlashlightMesh ? FlashlightMesh : Super::GetPhysicsComponent();
}

void ASFW_Flashlight::OnEquipped(ACharacter* NewOwnerChar)
{
	// Base handles attach + hiding root, and clears physics on root
	Super::OnEquipped(NewOwnerChar);

	// Ensure flashlight body is non-physical while in hand
	if (FlashlightMesh)
	{
		FlashlightMesh->SetSimulatePhysics(false);
		FlashlightMesh->SetEnableGravity(false);
		FlashlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FlashlightMesh->SetVisibility(true, true);
	}

	ApplyLightState();
}

void ASFW_Flashlight::OnUnequipped()
{
	// Turn off beam
	if (Spot)
	{
		Spot->SetVisibility(false);
		Spot->SetIntensity(0.f);
	}

	// Also ensure we are not simulating while hidden in inventory
	if (FlashlightMesh)
	{
		FlashlightMesh->SetSimulatePhysics(false);
		FlashlightMesh->SetEnableGravity(false);
		FlashlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Super::OnUnequipped();
}

void ASFW_Flashlight::PrimaryUse()
{
	UE_LOG(LogTemp, Log, TEXT("Flashlight::PrimaryUse (On=%d)"), bIsOn ? 1 : 0);
	ToggleLight();
}

EHeldItemType ASFW_Flashlight::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::Flashlight;
}

void ASFW_Flashlight::ToggleLight()
{
	UE_LOG(LogTemp, Log, TEXT("Flashlight::ToggleLight -> %s"), bIsOn ? TEXT("OFF") : TEXT("ON"));
	SetLightEnabled(!bIsOn);
}

void ASFW_Flashlight::SetLightEnabled(bool bEnable)
{
	if (!HasAuthority())
	{
		Server_SetLightEnabled(bEnable);
		return;
	}

	bIsOn = bEnable;
	ApplyLightState();
	Multicast_PlayToggleSFX(bIsOn);
}

void ASFW_Flashlight::Server_SetLightEnabled_Implementation(bool bEnable)
{
	bIsOn = bEnable;
	ApplyLightState();
	Multicast_PlayToggleSFX(bIsOn);
}

void ASFW_Flashlight::OnRep_IsOn()
{
	ApplyLightState();
}

void ASFW_Flashlight::ApplyLightState()
{
	const bool bVis = bIsOn;

	if (Spot)
	{
		Spot->SetVisibility(bVis);
		Spot->SetIntensity(bVis ? OnIntensity : 0.f);
		Spot->SetInnerConeAngle(InnerCone);
		Spot->SetOuterConeAngle(OuterCone);
		Spot->SetAttenuationRadius(AttenuationRadius);

		if (IESProfile)
		{
			Spot->SetIESTexture(IESProfile);
			Spot->bUseIESBrightness = bUseIESBrightness;
			Spot->IESBrightnessScale = IESBrightnessScale;
		}
	}
}

void ASFW_Flashlight::Multicast_PlayToggleSFX_Implementation(bool bEnable)
{
	USoundBase* SFX = bEnable ? ToggleOnSFX : ToggleOffSFX;
	if (SFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SFX, GetActorLocation());
	}
}
