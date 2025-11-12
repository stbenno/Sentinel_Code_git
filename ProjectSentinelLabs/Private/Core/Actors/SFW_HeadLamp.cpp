// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Actors/SFW_HeadLamp.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#include "Core/Components/SFW_EquipmentManagerComponent.h"

ASFW_HeadLamp::ASFW_HeadLamp()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// Visible mesh for the headlamp
	HeadlampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadlampMesh"));

	// Attach to whatever root EquippableBase created (skeletal Mesh)
	if (USceneComponent* Parent = GetRootComponent())
	{
		HeadlampMesh->SetupAttachment(Parent);
	}
	else
	{
		SetRootComponent(HeadlampMesh);
	}

	HeadlampMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadlampMesh->SetCastShadow(false);
	HeadlampMesh->SetIsReplicated(true);
	HeadlampMesh->SetVisibility(false);

	// Light attached to the headlamp mesh
	Lamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(HeadlampMesh);
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->bUseInverseSquaredFalloff = false;
	Lamp->SetIntensityUnits(IntensityUnits);
	Lamp->SetAttenuationRadius(AttenuationRadius);
	Lamp->SetInnerConeAngle(InnerCone);
	Lamp->SetOuterConeAngle(OuterCone);
	Lamp->SetIntensity(0.f);
	Lamp->SetVisibility(false);

	if (IESProfile)
	{
		Lamp->SetIESTexture(IESProfile);
		Lamp->bUseIESBrightness = bUseIESBrightness;
		Lamp->IESBrightnessScale = IESBrightnessScale;
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ASFW_HeadLamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASFW_HeadLamp, bLampEnabled);
}

UPrimitiveComponent* ASFW_HeadLamp::GetPhysicsComponent() const
{
	// When dropped, simulate the static mesh, not the skeletal root
	return HeadlampMesh ? HeadlampMesh : Super::GetPhysicsComponent();
}

void ASFW_HeadLamp::OnEquipped(ACharacter* NewOwnerChar)
{
	// Base class: set owner, attach actor to HeadSocketName, disable physics
	Super::OnEquipped(NewOwnerChar);

	// Make visible on head
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);

	if (HeadlampMesh)
	{
		HeadlampMesh->SetVisibility(true, true);
		HeadlampMesh->SetRelativeLocation(AttachOffset);
		HeadlampMesh->SetRelativeRotation(AttachRotation);
		HeadlampMesh->SetRelativeScale3D(AttachScale);
	}

	ApplyLightState();
}

void ASFW_HeadLamp::OnUnequipped()
{
	if (Lamp)
	{
		Lamp->SetVisibility(false);
		Lamp->SetIntensity(0.f);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	Super::OnUnequipped();
}

void ASFW_HeadLamp::Interact_Implementation(AController* InstigatorController)
{
	// Special-case: this does NOT go into handheld inventory.
	// It becomes the dedicated headlamp for this player.
	if (!HasAuthority() || !InstigatorController)
	{
		return;
	}

	ACharacter* Char = Cast<ACharacter>(InstigatorController->GetPawn());
	if (!Char)
	{
		return;
	}

	USFW_EquipmentManagerComponent* Equip =
		Char->FindComponentByClass<USFW_EquipmentManagerComponent>();
	if (!Equip)
	{
		return;
	}

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->SetSimulatePhysics(false);
		Phys->SetEnableGravity(false);
		Phys->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetOwner(Char);

	// Register as this player’s headlamp and attach
	Equip->SetHeadLamp(this);
	OnEquipped(Char);
}

void ASFW_HeadLamp::ToggleLamp()
{
	SetLampEnabled(!bLampEnabled);
}

void ASFW_HeadLamp::SetLampEnabled(bool bEnabled)
{
	if (!HasAuthority())
	{
		Server_SetLampEnabled(bEnabled);
		return;
	}

	bLampEnabled = bEnabled;
	ApplyLightState();
	Multicast_PlayToggleSFX(bLampEnabled);
}

void ASFW_HeadLamp::Server_SetLampEnabled_Implementation(bool bEnabled)
{
	bLampEnabled = bEnabled;
	ApplyLightState();
	Multicast_PlayToggleSFX(bLampEnabled);
}

void ASFW_HeadLamp::OnRep_LampEnabled()
{
	// Clients just mirror visuals; SFX comes from multicast
	ApplyLightState();
}

void ASFW_HeadLamp::ApplyLightState()
{
	const bool bVis = bLampEnabled;

	if (Lamp)
	{
		Lamp->SetVisibility(bVis);
		Lamp->SetIntensity(bVis ? OnIntensity : 0.f);
		Lamp->SetInnerConeAngle(InnerCone);
		Lamp->SetOuterConeAngle(OuterCone);
		Lamp->SetAttenuationRadius(AttenuationRadius);
	}
}

void ASFW_HeadLamp::Multicast_PlayToggleSFX_Implementation(bool bNewEnabled)
{
	USoundBase* S = bNewEnabled ? ToggleOnSFX : ToggleOffSFX;
	if (S)
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation());
	}
}
