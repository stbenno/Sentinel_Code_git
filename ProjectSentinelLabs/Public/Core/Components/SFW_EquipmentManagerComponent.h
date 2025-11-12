// SFW_EquipmentManagerComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h"   // EHeldItemType, EEquipState
#include "SFW_EquipmentManagerComponent.generated.h"

class ASFW_EquippableBase;
class ACharacter;
class ASFW_EMFDevice;

/**
 * Owns the player's carried items, hand item, and headlamp.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTSENTINELLABS_API USFW_EquipmentManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFW_EquipmentManagerComponent();

	/** Max number of handheld inventory slots. */
	static constexpr int32 kMaxInventorySlots = 3;

	// ~UActorComponent
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// ~

public:
	/** All handheld items (some slots may be null). */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ASFW_EquippableBase>> Inventory;

	/** Currently shown/equipped hand item (also lives in Inventory). */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveHandItem)
	TObjectPtr<ASFW_EquippableBase> ActiveHandItem = nullptr;

	/** Dedicated headlamp reference (not in Inventory). */
	UPROPERTY(Replicated)
	TObjectPtr<ASFW_EquippableBase> HeadLampRef = nullptr;

	/** What the animator should treat as the current handheld type. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Equipment|Anim")
	EHeldItemType HeldItem = EHeldItemType::None;

	/** High-level equip state used by the animator. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Equipment|Anim")
	EEquipState Equip = EEquipState::Idle;

	// ---- Queries ----
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool HasFreeInventorySlot(int32& OutIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool IsInventoryFull() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool IsHandEmpty() const { return ActiveHandItem == nullptr; }

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	ASFW_EquippableBase* GetItemInSlot(int32 Index) const;

	/** Convenience getters for AnimInstance / BP. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Anim")
	EHeldItemType GetHeldItemType() const { return HeldItem; }

	UFUNCTION(BlueprintPure, Category = "Equipment|Anim")
	EEquipState GetEquipState() const { return Equip; }

	/** Voice / Radio: does this inventory grant radio comms? */
	UFUNCTION(BlueprintPure, Category = "Equipment|Voice")
	bool CanUseRadioComms() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	ASFW_EMFDevice* FindEMF() const;

	// ---- Core actions (server auth) ----
	UFUNCTION(Server, Reliable)
	void Server_AddItemToInventory(ASFW_EquippableBase* Item, bool bAutoEquipIfHandEmpty);

	UFUNCTION(Server, Reliable)
	void Server_EquipSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_UnequipActive();

	UFUNCTION(Server, Reliable)
	void Server_DropActive();

	/** New: place the current active item (for REM-POD etc.). */
	UFUNCTION(Server, Reliable)
	void Server_PlaceActive();

	// Optional tunables
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Place")
	float PlaceTraceDistance = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Place")
	float MaxPlaceSlopeDegrees = 40.f;   // how steep is still "flat"

	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Place")
	float PlaceHeightOffset = 2.f;       // small lift off the surface

	/** Assign headlamp actor after pickup. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void SetHeadLamp(ASFW_EquippableBase* InHeadLamp);

	/** Toggle headlamp on/off (server). */
	UFUNCTION(Server, Reliable)
	void Server_ToggleHeadLamp();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UseActiveLocal();

protected:
	// RepNotify
	UFUNCTION()
	void OnRep_ActiveHandItem();

	/** Applies visibility/attachment when ActiveHandItem changes. */
	void ApplyActiveVisuals();

private:
	bool IsValidSlotIndex(int32 Index) const { return Index >= 0 && Index < kMaxInventorySlots; }
	bool SetItemInSlot_Internal(int32 Index, ASFW_EquippableBase* Item);
	bool ClearSlot_Internal(int32 Index);

	/** Cached owner character (used for attachment and traces). */
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerChar = nullptr;
};
