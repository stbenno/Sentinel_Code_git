// SFW_REMPod.cpp

#include "Core/Actors/SFW_REMPod.h"

#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASFW_REMPod::ASFW_REMPod()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// Hold in EMF-style hand by default
	EquipSlot = ESFWEquipSlot::Hand_EMF;

	HumAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudioComp"));
	HumAudioComp->SetupAttachment(PodMesh);
	HumAudioComp->bAutoActivate = false;

	bIsActive = false;
}

void ASFW_REMPod::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASFW_REMPod, bIsActive);
}

UPrimitiveComponent* ASFW_REMPod::GetPhysicsComponent() const
{
	// Use the static mesh as physics body
	return PodMesh ? PodMesh : Super::GetPhysicsComponent();
}

EHeldItemType ASFW_REMPod::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::REMPod;
}

void ASFW_REMPod::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);

	if (PodMesh)
	{
		PodMesh->SetSimulatePhysics(false);
		PodMesh->SetEnableGravity(false);
		PodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PodMesh->SetVisibility(true, true);
	}

	ApplyActiveState();
}

void ASFW_REMPod::OnUnequipped()
{
	// Hide and stop audio while stored in inventory
	if (PodMesh)
	{
		PodMesh->SetSimulatePhysics(false);
		PodMesh->SetEnableGravity(false);
		PodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (HumAudioComp && HumAudioComp->IsPlaying())
	{
		HumAudioComp->Stop();
	}

	Super::OnUnequipped();
}

void ASFW_REMPod::PrimaryUse()
{
	// Toggle power from local owner when held
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->IsPlayerControlled() || !OwnerChar->IsLocallyControlled())
	{
		return;
	}

	SetActive(!bIsActive);
}

void ASFW_REMPod::SetActive(bool bEnable)
{
	if (!HasAuthority())
	{
		Server_SetActive(bEnable);
		return;
	}

	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_REMPod::Server_SetActive_Implementation(bool bEnable)
{
	bIsActive = bEnable;
	ApplyActiveState();
}

void ASFW_REMPod::OnRep_IsActive()
{
	ApplyActiveState();
}

void ASFW_REMPod::OnPlaced(const FTransform& WorldTransform)
{
	// Base: detach, move, enable collision, disable physics
	Super::OnPlaced(WorldTransform);

	// Optional extras for placed state. Keep active state visuals in sync.
	ApplyActiveState();
}

void ASFW_REMPod::ApplyActiveState()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

	// Is the device attached to a character (held) vs placed in world?
	const bool bAttachedToChar = (GetAttachParentActor() && Cast<ACharacter>(GetAttachParentActor()) != nullptr);

	// Hum logic:
	// - Held: owner-local only to avoid double audio
	// - Placed: audible to everyone
	if (HumAudioComp)
	{
		if (bIsActive)
		{
			if (bAttachedToChar)
			{
				const bool bLocalOwner =
					OwnerChar &&
					OwnerChar->IsPlayerControlled() &&
					OwnerChar->IsLocallyControlled();

				if (bLocalOwner)
				{
					if (!HumAudioComp->IsPlaying()) HumAudioComp->Play();
				}
				else
				{
					if (HumAudioComp->IsPlaying()) HumAudioComp->Stop();
				}
			}
			else
			{
				// Placed in world: play everywhere
				if (!HumAudioComp->IsPlaying()) HumAudioComp->Play();
			}
		}
		else
		{
			if (HumAudioComp->IsPlaying()) HumAudioComp->Stop();
		}
	}

	// One-shot SFX on local owner when toggling while held
	const bool bLocalHeld =
		bAttachedToChar &&
		OwnerChar &&
		OwnerChar->IsPlayerControlled() &&
		OwnerChar->IsLocallyControlled();

	if (bLocalHeld)
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

	// TODO: drive LEDs or emissive here based on bIsActive if desired.
}
