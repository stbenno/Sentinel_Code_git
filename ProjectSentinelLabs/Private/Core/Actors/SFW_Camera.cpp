// Fill out your copyright notice in the Description page of Project Settings.


// SFW_Camera.cpp

#include "Core/Actors/SFW_Camera.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASFW_Camera::ASFW_Camera()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// One-handed tool grip for now
	EquipSlot = ESFWEquipSlot::Hand_Tool;

	// Root skeletal mesh comes from EquippableBase::Mesh

}

UPrimitiveComponent* ASFW_Camera::GetPhysicsComponent() const
{
	// Use the static mesh as physics body when dropped
	return CameraMesh ? CameraMesh : Super::GetPhysicsComponent();
}

EHeldItemType ASFW_Camera::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::Camera;
}

void ASFW_Camera::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);

	if (CameraMesh)
	{
		CameraMesh->SetSimulatePhysics(false);
		CameraMesh->SetEnableGravity(false);
		CameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CameraMesh->SetVisibility(true, true);
	}
}

void ASFW_Camera::OnUnequipped()
{
	if (CameraMesh)
	{
		CameraMesh->SetSimulatePhysics(false);
		CameraMesh->SetEnableGravity(false);
		CameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Super::OnUnequipped();
}

void ASFW_Camera::PrimaryUse()
{
	// Only the local owning player should initiate a shutter
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->IsPlayerControlled() || !OwnerChar->IsLocallyControlled())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Camera] PrimaryUse -> FireShutter (%s)"), *GetName());

	if (!HasAuthority())
	{
		Server_FireShutter();
	}
	else
	{
		HandleShutterFired_Server();
	}
}

void ASFW_Camera::Server_FireShutter_Implementation()
{
	HandleShutterFired_Server();
}

void ASFW_Camera::HandleShutterFired_Server()
{
	// TODO: later hook in actual gameplay (evidence capture, traces, etc.)

	// Cosmetic for everyone
	Multicast_PlayShutterFX();
}

void ASFW_Camera::Multicast_PlayShutterFX_Implementation()
{
	// Shutter click for all clients
	if (ShutterSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShutterSFX, GetActorLocation());
	}

	// Later: small light flash, camera recoil anim, etc.
}
