// SFW_EquipmentManagerComponent.cpp

#include "Core/Components/SFW_EquipmentManagerComponent.h"

#include "Core/Actors/SFW_EquippableBase.h"
#include "Core/Actors/SFW_HeadLamp.h"
#include "Core/Actors/SFW_EMFDevice.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "DrawDebugHelpers.h"

USFW_EquipmentManagerComponent::USFW_EquipmentManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Inventory.SetNumZeroed(kMaxInventorySlots);
}

void USFW_EquipmentManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<ACharacter>(GetOwner());
	if (Inventory.Num() != kMaxInventorySlots)
	{
		Inventory.SetNumZeroed(kMaxInventorySlots);
	}
}

void USFW_EquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USFW_EquipmentManagerComponent, Inventory);
	DOREPLIFETIME(USFW_EquipmentManagerComponent, ActiveHandItem);
	DOREPLIFETIME(USFW_EquipmentManagerComponent, HeadLampRef);
	DOREPLIFETIME(USFW_EquipmentManagerComponent, HeldItem);
	DOREPLIFETIME(USFW_EquipmentManagerComponent, Equip);
}

// ---------- Queries ----------

bool USFW_EquipmentManagerComponent::HasFreeInventorySlot(int32& OutIndex) const
{
	for (int32 i = 0; i < kMaxInventorySlots; ++i)
	{
		if (Inventory[i] == nullptr)
		{
			OutIndex = i;
			return true;
		}
	}
	OutIndex = INDEX_NONE;
	return false;
}

bool USFW_EquipmentManagerComponent::IsInventoryFull() const
{
	int32 Dummy;
	return !HasFreeInventorySlot(Dummy);
}

ASFW_EquippableBase* USFW_EquipmentManagerComponent::GetItemInSlot(int32 Index) const
{
	return IsValidSlotIndex(Index) ? Inventory[Index] : nullptr;
}

bool USFW_EquipmentManagerComponent::CanUseRadioComms() const
{
	for (ASFW_EquippableBase* Item : Inventory)
	{
		if (Item && Item->GrantsRadioComms())
		{
			return true;
		}
	}
	return false;
}

// ---------- Helpers ----------

bool USFW_EquipmentManagerComponent::SetItemInSlot_Internal(int32 Index, ASFW_EquippableBase* Item)
{
	if (!IsValidSlotIndex(Index)) return false;
	Inventory[Index] = Item;
	return true;
}

bool USFW_EquipmentManagerComponent::ClearSlot_Internal(int32 Index)
{
	if (!IsValidSlotIndex(Index)) return false;

	if (Inventory[Index] && Inventory[Index] == ActiveHandItem)
	{
		ActiveHandItem = nullptr;
	}
	Inventory[Index] = nullptr;
	return true;
}

void USFW_EquipmentManagerComponent::SetHeadLamp(ASFW_EquippableBase* InHeadLamp)
{
	HeadLampRef = InHeadLamp;
}

void USFW_EquipmentManagerComponent::OnRep_ActiveHandItem()
{
	ApplyActiveVisuals();
}

void USFW_EquipmentManagerComponent::ApplyActiveVisuals()
{
	UE_LOG(LogTemp, Log, TEXT("[EquipMgr] ApplyActiveVisuals Owner=%s ActiveHandItem=%s"),
		*GetOwner()->GetName(),
		ActiveHandItem ? *ActiveHandItem->GetName() : TEXT("NULL"));

	// Hide / detach everything first
	for (ASFW_EquippableBase* Item : Inventory)
	{
		if (Item)
		{
			Item->OnUnequipped();
		}
	}

	// Then show/attach the active item
	if (ActiveHandItem && OwnerChar)
	{
		UE_LOG(LogTemp, Log, TEXT("[EquipMgr] Equipping %s to %s"),
			*ActiveHandItem->GetName(),
			*OwnerChar->GetName());

		ActiveHandItem->OnEquipped(OwnerChar);
	}
}

ASFW_EMFDevice* USFW_EquipmentManagerComponent::FindEMF() const
{
	for (ASFW_EquippableBase* Item : Inventory)
	{
		if (!Item) continue;

		if (ASFW_EMFDevice* EMF = Cast<ASFW_EMFDevice>(Item))
		{
			return EMF;
		}
	}
	return nullptr;
}

// ---------- Server RPCs ----------

void USFW_EquipmentManagerComponent::Server_AddItemToInventory_Implementation(ASFW_EquippableBase* Item, bool bAutoEquipIfHandEmpty)
{
	UE_LOG(LogTemp, Log, TEXT("[EquipMgr] AddItemToInventory called on %s. Item=%s AutoEquip=%d"),
		*GetOwner()->GetName(),
		Item ? *Item->GetName() : TEXT("NULL"),
		bAutoEquipIfHandEmpty ? 1 : 0);

	if (!Item) return;

	int32 FreeIndex;
	if (!HasFreeInventorySlot(FreeIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipMgr] Inventory full for %s"), *GetOwner()->GetName());
		return;
	}

	SetItemInSlot_Internal(FreeIndex, Item);
	UE_LOG(LogTemp, Log, TEXT("[EquipMgr] Stored %s in slot %d"), *Item->GetName(), FreeIndex);

	if (bAutoEquipIfHandEmpty && ActiveHandItem == nullptr)
	{
		ActiveHandItem = Item;
		HeldItem = ActiveHandItem
			? ActiveHandItem->GetAnimHeldType()
			: EHeldItemType::None;
		Equip = EEquipState::Holding;

		UE_LOG(LogTemp, Log, TEXT("[EquipMgr] Auto-equipped %s as ActiveHandItem"), *Item->GetName());
	}

	ApplyActiveVisuals();
}

void USFW_EquipmentManagerComponent::Server_EquipSlot_Implementation(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex)) return;

	ASFW_EquippableBase* Candidate = Inventory[SlotIndex];
	if (!Candidate) return;

	ActiveHandItem = Candidate;

	HeldItem = ActiveHandItem
		? ActiveHandItem->GetAnimHeldType()
		: EHeldItemType::None;
	Equip = ActiveHandItem ? EEquipState::Holding : EEquipState::Idle;

	ApplyActiveVisuals();
}

void USFW_EquipmentManagerComponent::Server_UnequipActive_Implementation()
{
	ActiveHandItem = nullptr;
	HeldItem = EHeldItemType::None;
	Equip = EEquipState::Idle;

	ApplyActiveVisuals();
}

void USFW_EquipmentManagerComponent::Server_DropActive_Implementation()
{
	if (!ActiveHandItem || !OwnerChar)
	{
		return;
	}

	ASFW_EquippableBase* Dropped = ActiveHandItem;

	// Remove from inventory
	for (int32 i = 0; i < kMaxInventorySlots; ++i)
	{
		if (Inventory[i] == Dropped)
		{
			ClearSlot_Internal(i);
			break;
		}
	}

	ActiveHandItem = nullptr;
	HeldItem = EHeldItemType::None;
	Equip = EEquipState::Idle;

	ApplyActiveVisuals(); // unequips remaining items, not the dropped one

	// Compute drop location + toss from camera / eyes
	FVector EyeLoc;
	FRotator EyeRot;
	OwnerChar->GetActorEyesViewPoint(EyeLoc, EyeRot);

	const FVector Forward = EyeRot.Vector();
	const FVector DropLocation = EyeLoc + Forward * 80.f - FVector(0.f, 0.f, 20.f);
	const FVector TossVelocity = Forward * 150.f + FVector(0.f, 0.f, 50.f);

	// No longer owned for relevancy
	Dropped->SetOwner(nullptr);

	// Let the item configure its physics and visuals on all peers
	Dropped->Multicast_OnDropped(DropLocation, TossVelocity);
}

// In SFW_EquipmentManagerComponent.cpp

void USFW_EquipmentManagerComponent::Server_PlaceActive_Implementation()
{
	if (!OwnerChar || !ActiveHandItem)
	{
		return;
	}

	// Only allow items that opt into placement (REMPod returns true)
	if (!ActiveHandItem->CanBePlaced())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1) Trace from eyes forward
	FVector EyeLoc;
	FRotator EyeRot;
	OwnerChar->GetActorEyesViewPoint(EyeLoc, EyeRot);

	const FVector Start = EyeLoc;
	const FVector End = Start + EyeRot.Vector() * PlaceTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaceItem), false, OwnerChar);

	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		// nothing hit -> invalid placement
		return;
	}

	// 2) Filter to world geometry only (so you can't place on players / items)
	ECollisionChannel HitChannel = ECC_WorldStatic;
	if (Hit.Component.IsValid())
	{
		HitChannel = Hit.Component->GetCollisionObjectType();
	}

	if (HitChannel != ECC_WorldStatic && HitChannel != ECC_WorldDynamic)
	{
		return; // not ground / level geometry
	}

	// 3) Check surface slope vs world up
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float DotUp = FVector::DotProduct(Normal, FVector::UpVector);

	const float CosMaxSlope =
		FMath::Cos(FMath::DegreesToRadians(MaxPlaceSlopeDegrees));

	if (DotUp < CosMaxSlope)
	{
		// too steep -> e.g. wall, stairs, sharp slope
		return;
	}

	// 4) Build final placement transform
	const FVector PlaceLocation = Hit.Location + Normal * PlaceHeightOffset;

	// Keep item upright in world, yaw aligned to character
	const FRotator PlaceRot(0.f, OwnerChar->GetActorRotation().Yaw, 0.f);

	FTransform PlaceXform;
	PlaceXform.SetLocation(PlaceLocation);
	PlaceXform.SetRotation(PlaceRot.Quaternion());
	PlaceXform.SetScale3D(FVector::OneVector);

	// 5) Let the item transition into "placed" state
	// Use whatever you already call here:
	// - if you made a multicast: ActiveHandItem->Multicast_OnPlaced(PlaceXform);
	// - if you just had OnPlaced and call it on server then replicate state, do that instead.
	ActiveHandItem->OnPlaced(PlaceXform);

	// 6) Remove from inventory / hand
	for (int32 i = 0; i < kMaxInventorySlots; ++i)
	{
		if (Inventory[i] == ActiveHandItem)
		{
			ClearSlot_Internal(i);
			break;
		}
	}

	ActiveHandItem = nullptr;
	HeldItem = EHeldItemType::None;
	Equip = EEquipState::Idle;

	ApplyActiveVisuals();
}

void USFW_EquipmentManagerComponent::UseActiveLocal()
{
	if (!ActiveHandItem)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UseActiveLocal: Using %s"), *ActiveHandItem->GetName());

	ActiveHandItem->PrimaryUse();
}

void USFW_EquipmentManagerComponent::Server_ToggleHeadLamp_Implementation()
{
	if (ASFW_HeadLamp* Lamp = Cast<ASFW_HeadLamp>(HeadLampRef))
	{
		Lamp->ToggleLamp();
	}
}
