// Fill out your copyright notice in the Description page of Project Settings.


// SFW_SoundSensor.cpp

#include "Core/Actors/SFW_SoundSensor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASFW_SoundSensor::ASFW_SoundSensor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// Generic tool slot for now
	EquipSlot = ESFWEquipSlot::Hand_Tool;

	LoopAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudioComp"));
	LoopAudioComp->SetupAttachment(SensorMesh);
	LoopAudioComp->bAutoActivate = false;

	bIsActive = false;
}

void ASFW_SoundSensor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_SoundSensor, bIsActive);
}

UPrimitiveComponent* ASFW_SoundSensor::GetPhysicsComponent() const
{
	// Use the static mesh as physics body when dropped
	return SensorMesh ? SensorMesh : Super::GetPhysicsComponent();
}

EHeldItemType ASFW_SoundSensor::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::SoundSensor;
}

void ASFW_SoundSensor::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);

	if (SensorMesh)
	{
		SensorMesh->SetSimulatePhysics(false);
		SensorMesh->SetEnableGravity(false);
		SensorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SensorMesh->SetVisibility(true, true);
	}

	ApplyActiveState();
}

void ASFW_SoundSensor::OnUnequipped()
{
	if (SensorMesh)
	{
		SensorMesh->SetSimulatePhysics(false);
		SensorMesh->SetEnableGravity(false);
		SensorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (LoopAudioComp && LoopAudioComp->IsPlaying())
	{
		LoopAudioComp->Stop();
	}

	Super::OnUnequipped();
}

void ASFW_SoundSensor::PrimaryUse()
{
	// Local owner toggles power
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->IsPlayerControlled() || !OwnerChar->IsLocallyControlled())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SoundSensor] PrimaryUse toggle from %s"), *GetName());

	SetActive(!bIsActive);
}

void ASFW_SoundSensor::SetActive(bool bEnable)
{
	if (!HasAuthority())
	{
		Server_SetActive(bEnable);
		return;
	}

	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_SoundSensor::Server_SetActive_Implementation(bool bEnable)
{
	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_SoundSensor::OnRep_IsActive()
{
	ApplyActiveState();
}

void ASFW_SoundSensor::ApplyActiveState()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	const bool bLocalOwner =
		OwnerChar &&
		OwnerChar->IsPlayerControlled() &&
		OwnerChar->IsLocallyControlled();

	// Looping audio only for local owner (optional)
	if (LoopAudioComp)
	{
		if (bIsActive)
		{
			if (bLocalOwner && !LoopAudioComp->IsPlaying())
			{
				LoopAudioComp->Play();
			}
			if (!bLocalOwner && LoopAudioComp->IsPlaying())
			{
				LoopAudioComp->Stop();
			}
		}
		else
		{
			if (LoopAudioComp->IsPlaying())
			{
				LoopAudioComp->Stop();
			}
		}
	}

	// One-shot power SFX for local owner
	if (bLocalOwner)
	{
		if (bIsActive && PowerOnSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PowerOnSFX, GetActorLocation());
		}
		else if (!bIsActive && PowerOffSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PowerOffSFX, GetActorLocation());
		}
	}

	// Later: drive visualization / world listening here.
}

