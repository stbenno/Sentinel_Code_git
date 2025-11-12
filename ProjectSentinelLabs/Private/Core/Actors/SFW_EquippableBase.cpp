#include "Core/Actors/SFW_EquippableBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Core/Components/SFW_EquipmentManagerComponent.h"

ASFW_EquippableBase::ASFW_EquippableBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetIsReplicated(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(RootComponent);
	InteractionCollision->SetBoxExtent(FVector(10.f));

	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ASFW_EquippableBase::BeginPlay()
{
	Super::BeginPlay();

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		InitialPhysicsRelativeTransform = Phys->GetRelativeTransform();
		bHasCachedPhysicsRelativeTransform = true;
	}
}

// ---------- Equip / Unequip / Drop ----------

void ASFW_EquippableBase::OnEquipped(ACharacter* NewOwnerChar)
{
	if (NewOwnerChar)
	{
		SetOwner(NewOwnerChar);
	}

	AttachToCharacter(NewOwnerChar, GetAttachSocketName());

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->SetSimulatePhysics(false);
		Phys->SetEnableGravity(false);
		Phys->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (bHasCachedPhysicsRelativeTransform)
		{
			if (Phys != GetRootComponent())
			{
				Phys->AttachToComponent(
					GetRootComponent(),
					FAttachmentTransformRules::KeepRelativeTransform);
			}

			Phys->SetRelativeTransform(InitialPhysicsRelativeTransform);
		}
	}

	if (InteractionCollision)
	{
		InteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetActorHiddenInGame(false);
}

void ASFW_EquippableBase::OnUnequipped()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	if (InteractionCollision)
	{
		InteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	DetachFromCharacter();
}

void ASFW_EquippableBase::OnDropped(const FVector& DropLocation, const FVector& TossVelocity)
{
	DetachFromCharacter();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Phys->SetWorldLocation(DropLocation);

		Phys->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Phys->SetCollisionResponseToAllChannels(ECR_Block);

		Phys->SetSimulatePhysics(true);
		Phys->SetEnableGravity(true);

		Phys->SetPhysicsLinearVelocity(TossVelocity, true);
		Phys->WakeAllRigidBodies();
	}
	else
	{
		SetActorLocation(DropLocation);
	}

	if (InteractionCollision)
	{
		InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ASFW_EquippableBase::Multicast_OnDropped_Implementation(const FVector& DropLocation, const FVector& TossVelocity)
{
	OnDropped(DropLocation, TossVelocity);
}

// ---------- Placement ----------



void ASFW_EquippableBase::OnPlaced(const FTransform& WorldTransform)
{
	DetachFromCharacter();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Phys->SetWorldTransform(WorldTransform);

		Phys->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Phys->SetCollisionResponseToAllChannels(ECR_Block);

		// Placed items are typically static
		Phys->SetSimulatePhysics(false);
		Phys->SetEnableGravity(false);
	}
	else
	{
		SetActorTransform(WorldTransform);
	}

	if (InteractionCollision)
	{
		InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ASFW_EquippableBase::Multicast_OnPlaced_Implementation(const FTransform& WorldTransform)
{
	OnPlaced(WorldTransform);
}

// ---------- Attach / Helpers ----------

void ASFW_EquippableBase::AttachToCharacter(ACharacter* Char, FName Socket)
{
	if (!Char) return;

	if (USkeletalMeshComponent* CMesh = Char->GetMesh())
	{
		const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, true);
		AttachToComponent(CMesh, Rules, Socket);
	}
}

void ASFW_EquippableBase::DetachFromCharacter()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

UPrimitiveComponent* ASFW_EquippableBase::GetPhysicsComponent() const
{
	return Cast<UPrimitiveComponent>(GetRootComponent());
}

FName ASFW_EquippableBase::GetAttachSocketName() const
{
	switch (EquipSlot)
	{
	case ESFWEquipSlot::Hand_Light:  return TEXT("hand_r_Light");
	case ESFWEquipSlot::Hand_EMF:    return TEXT("hand_r_EMF");
	case ESFWEquipSlot::Hand_Tool:   return TEXT("hand_r_Tool");
	case ESFWEquipSlot::Hand_Thermo: return TEXT("hand_r_Thermo");
	case ESFWEquipSlot::Head:        return TEXT("head_LampSocket");
	default:                         return TEXT("hand_r_Tool");
	}
}

// ---------- Interact ----------

FText ASFW_EquippableBase::GetPromptText_Implementation() const
{
	if (!DisplayName.IsEmpty())
	{
		return FText::Format(
			NSLOCTEXT("Equippable", "PickupFmt", "Pick up {0}"),
			DisplayName);
	}

	return NSLOCTEXT("Equippable", "PickupDefault", "Pick up item");
}

void ASFW_EquippableBase::Interact_Implementation(AController* InstigatorController)
{
	UE_LOG(LogTemp, Log, TEXT("Interact on %s (Auth=%d)"),
		*GetName(),
		HasAuthority() ? 1 : 0);

	if (!InstigatorController)
	{
		return;
	}

	ACharacter* Char = Cast<ACharacter>(InstigatorController->GetPawn());
	if (!Char) return;

	USFW_EquipmentManagerComponent* Equip =
		Char->FindComponentByClass<USFW_EquipmentManagerComponent>();
	if (!Equip) return;

	if (!HasAuthority())
	{
		Equip->Server_AddItemToInventory(this, /*bAutoEquipIfHandEmpty=*/true);
		return;
	}

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->SetSimulatePhysics(false);
		Phys->SetEnableGravity(false);
		Phys->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetOwner(Char);

	Equip->Server_AddItemToInventory(this, /*bAutoEquipIfHandEmpty=*/true);
}
