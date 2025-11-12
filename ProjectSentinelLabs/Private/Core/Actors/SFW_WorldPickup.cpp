// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Actors/SFW_WorldPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Core/Actors/Interface/SFW_InteractableInterface.h"
#include "Core/Components/SFW_EquipmentManagerComponent.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "Core/Actors/SFW_HeadLamp.h"

ASFW_WorldPickup::ASFW_WorldPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(PickupMesh);
	PickupMesh->SetSimulatePhysics(true);
	PickupMesh->SetIsReplicated(true);

	UseBox = CreateDefaultSubobject<UBoxComponent>(TEXT("UseBox"));
	UseBox->SetupAttachment(PickupMesh);
	UseBox->SetBoxExtent(FVector(40.f));
	UseBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UseBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	UseBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ItemDisplayName = FText::FromString(TEXT("Headlamp"));
}

FText ASFW_WorldPickup::GetPromptText_Implementation() const
{
	return FText::Format(FText::FromString(TEXT("Pick up {0}")), ItemDisplayName);
}

void ASFW_WorldPickup::Interact_Implementation(AController* InstigatorController)
{
	if (!InstigatorController) return;

	// Only the server handles giving items
	if (HasAuthority())
	{
		Server_GiveTo(InstigatorController);
	}
}

void ASFW_WorldPickup::Server_GiveTo_Implementation(AController* InstigatorController)
{
	if (!InstigatorController || !ItemClass) return;

	ACharacter* Char = Cast<ACharacter>(InstigatorController->GetPawn());
	if (!Char) return;

	USFW_EquipmentManagerComponent* Equip = Char->FindComponentByClass<USFW_EquipmentManagerComponent>();
	if (!Equip) return;

	// Spawn the actual equippable
	FActorSpawnParameters Params;
	Params.Owner = Char;
	Params.Instigator = Char;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	

	ASFW_EquippableBase* Spawned = GetWorld()->SpawnActor<ASFW_EquippableBase>(ItemClass, GetActorTransform(), Params);
	if (!Spawned) return;

	// Headlamp = special dedicated slot (not a handheld inventory item)
	if (Spawned->IsA(ASFW_HeadLamp::StaticClass()))
	{
		ASFW_HeadLamp* Lamp = CastChecked<ASFW_HeadLamp>(Spawned);
		Equip->SetHeadLamp(Lamp);
		Lamp->OnEquipped(Char);                 // attaches to "head_LampSocket"
	}
	else
	{
		// Handheld path (flashlight, etc.) → goes into inventory and auto-equips if empty
		Equip->Server_AddItemToInventory(Spawned, /*bAutoEquipIfHandEmpty=*/true);
	}

	// Remove the pickup from the world
	Destroy();
}