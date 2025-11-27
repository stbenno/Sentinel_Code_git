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

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Mesh->SetIsReplicated(true);
	Mesh->SetMobility(EComponentMobility::Movable);

	// Start in a non-physics, non-colliding state for “in hand”
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetEnableGravity(false);

	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(Mesh);
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
	UE_LOG(LogTemp, Log, TEXT("[Equippable] OnEquipped %s -> Owner %s Slot=%d Socket=%s"),
		*GetName(),
		*GetNameSafe(NewOwnerChar),
		(int32)EquipSlot,
		*GetAttachSocketName().ToString());

	if (NewOwnerChar)
	{
		SetOwner(NewOwnerChar);
		AttachToCharacter(NewOwnerChar, GetAttachSocketName());
	}

	// In hand: no physics, no collision
	Mesh->SetSimulatePhysics(false);
	Mesh->SetEnableGravity(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
		InteractionCollision->SetHiddenInGame(true);
	}

	// Detach the ACTOR, not an individual component
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}


void ASFW_EquippableBase::OnDropped(const FVector& DropLocation, const FVector& TossVelocity)
{
	DetachFromCharacter();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Work directly on the mesh (root)
	if (Mesh)
	{
		Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		// Put it where we want
		Mesh->SetWorldLocation(DropLocation);

		// Give it a sane collision preset for physics actors
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);

		// Physics on
		Mesh->SetSimulatePhysics(true);
		Mesh->SetEnableGravity(true);

		Mesh->SetPhysicsLinearVelocity(TossVelocity, true);
		Mesh->WakeAllRigidBodies();

		UE_LOG(LogTemp, Log, TEXT("[Equippable] OnDropped %s Sim=%d Grav=%d"),
			*GetName(),
			Mesh->IsSimulatingPhysics() ? 1 : 0,
			Mesh->IsGravityEnabled() ? 1 : 0);
	}
	else
	{
		// Fallback, but you shouldn't hit this
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
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	SetActorTransform(WorldTransform);

	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Phys->SetCollisionResponseToAllChannels(ECR_Block);

		Phys->SetSimulatePhysics(false);
		Phys->SetEnableGravity(false);
	}

	if (InteractionCollision)
	{
		InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InteractionCollision->SetHiddenInGame(true);
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

		SetActorRelativeTransform(HandOffset);
	}
}

void ASFW_EquippableBase::DetachFromCharacter()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

UPrimitiveComponent* ASFW_EquippableBase::GetPhysicsComponent() const
{
	return Mesh;
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
	UE_LOG(LogTemp, Log, TEXT("[Equippable] Interact on %s (Auth=%d, Controller=%s)"),
		*GetName(),
		HasAuthority() ? 1 : 0,
		*GetNameSafe(InstigatorController));

	if (!InstigatorController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equippable] Interact aborted: no InstigatorController"));
		return;
	}

	ACharacter* Char = Cast<ACharacter>(InstigatorController->GetPawn());
	if (!Char)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equippable] Interact aborted: Controller has no pawn"));
		return;
	}

	USFW_EquipmentManagerComponent* Equip =
		Char->FindComponentByClass<USFW_EquipmentManagerComponent>();
	if (!Equip)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equippable] Interact aborted: No EquipmentManager on %s"),
			*Char->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Equippable] Interact: calling AddItemToInventory on %s (Auth=%d)"),
		*Char->GetName(),
		HasAuthority() ? 1 : 0);

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[Equippable] Client path: calling Server_AddItemToInventory"));
		Equip->Server_AddItemToInventory(this, /*bAutoEquipIfHandEmpty=*/true);
		return;
	}

	// Server path
	if (UPrimitiveComponent* Phys = GetPhysicsComponent())
	{
		Phys->SetSimulatePhysics(false);
		Phys->SetEnableGravity(false);
		Phys->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetOwner(Char);

	UE_LOG(LogTemp, Log, TEXT("[Equippable] Server path: calling Server_AddItemToInventory directly"));
	Equip->Server_AddItemToInventory(this, /*bAutoEquipIfHandEmpty=*/true);
}

