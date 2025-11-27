// Fill out your copyright notice in the Description page of Project Settings.


// SFW_GeigerCounter.cpp

#include "Core/Actors/SFW_GeigerCounter.h"

#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASFW_GeigerCounter::ASFW_GeigerCounter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// Use EMF-style hand slot for now
	EquipSlot = ESFWEquipSlot::Hand_EMF;

	LoopAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudioComp"));
	LoopAudioComp->SetupAttachment(GeigerMesh);
	LoopAudioComp->bAutoActivate = false;

	bIsActive = false;
}

void ASFW_GeigerCounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_GeigerCounter, bIsActive);
}

UPrimitiveComponent* ASFW_GeigerCounter::GetPhysicsComponent() const
{
	// Use the static mesh as physics body when dropped
	return GeigerMesh ? GeigerMesh : Super::GetPhysicsComponent();
}

EHeldItemType ASFW_GeigerCounter::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::Geiger;
}

void ASFW_GeigerCounter::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);

	if (GeigerMesh)
	{
		GeigerMesh->SetSimulatePhysics(false);
		GeigerMesh->SetEnableGravity(false);
		GeigerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GeigerMesh->SetVisibility(true, true);
	}

	ApplyActiveState();
}

void ASFW_GeigerCounter::OnUnequipped()
{
	if (GeigerMesh)
	{
		GeigerMesh->SetSimulatePhysics(false);
		GeigerMesh->SetEnableGravity(false);
		GeigerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (LoopAudioComp && LoopAudioComp->IsPlaying())
	{
		LoopAudioComp->Stop();
	}

	Super::OnUnequipped();
}

void ASFW_GeigerCounter::PrimaryUse()
{
	// Local owner toggles power
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->IsPlayerControlled() || !OwnerChar->IsLocallyControlled())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Geiger] PrimaryUse toggle from %s"), *GetName());

	SetActive(!bIsActive);
}

void ASFW_GeigerCounter::SetActive(bool bEnable)
{
	if (!HasAuthority())
	{
		Server_SetActive(bEnable);
		return;
	}

	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_GeigerCounter::Server_SetActive_Implementation(bool bEnable)
{
	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_GeigerCounter::OnRep_IsActive()
{
	ApplyActiveState();
}

void ASFW_GeigerCounter::ApplyActiveState()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	const bool bLocalOwner =
		OwnerChar &&
		OwnerChar->IsPlayerControlled() &&
		OwnerChar->IsLocallyControlled();

	// Looping tick/hiss only for local owner (optional)
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

	// Later: vary tick rate based on anomaly / radiation level.
}


