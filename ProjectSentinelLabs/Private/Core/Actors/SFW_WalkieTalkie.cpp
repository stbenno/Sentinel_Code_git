// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Actors/SFW_WalkieTalkie.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"

ASFW_WalkieTalkie::ASFW_WalkieTalkie()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// Root skeletal mesh comes from ASFW_EquippableBase (Mesh).
	// This is the static mesh body the player sees and that uses physics when dropped.
	WalkieMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WalkieMesh"));
	WalkieMesh->SetupAttachment(GetMesh());
	WalkieMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WalkieMesh->SetCastShadow(true);
	WalkieMesh->SetIsReplicated(true);
	WalkieMesh->SetVisibility(true, true);

	EquipSlot = ESFWEquipSlot::Hand_Tool;
	

	//SetActorHiddenInGame(true);
	//SetActorEnableCollision(false);
}

UPrimitiveComponent* ASFW_WalkieTalkie::GetPhysicsComponent() const
{
	// Use the static mesh as the physics body for drop / pickup
	return WalkieMesh ? WalkieMesh : Super::GetPhysicsComponent();
}

void ASFW_WalkieTalkie::OnEquipped(ACharacter* NewOwnerChar)
{
	// Base handles attach to the character mesh and clears physics on the root
	Super::OnEquipped(NewOwnerChar);

	// Ensure the walkie body is non-physical while in hand
	if (WalkieMesh)
	{
		WalkieMesh->SetSimulatePhysics(false);
		WalkieMesh->SetEnableGravity(false);
		WalkieMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WalkieMesh->SetVisibility(true, true);
	}
}

void ASFW_WalkieTalkie::OnUnequipped()
{
	// Hidden in inventory, no physics
	if (WalkieMesh)
	{
		WalkieMesh->SetSimulatePhysics(false);
		WalkieMesh->SetEnableGravity(false);
		WalkieMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	Super::OnUnequipped();
}

void ASFW_WalkieTalkie::PrimaryUse()
{
	// Placeholder for push-to-talk logic
	UE_LOG(LogTemp, Log, TEXT("WalkieTalkie::PrimaryUse() called (push-to-talk placeholder)"));
}

EHeldItemType ASFW_WalkieTalkie::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::WalkieTalkie;
}


