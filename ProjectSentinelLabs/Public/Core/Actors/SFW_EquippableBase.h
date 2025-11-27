#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Actors/Interface/SFW_InteractableInterface.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h"
#include "SFW_EquippableBase.generated.h"

class AController;
class ACharacter;
class USkeletalMeshComponent;
class UPrimitiveComponent;
class UBoxComponent;

UENUM(BlueprintType)
enum class ESFWEquipSlot : uint8
{
	None        UMETA(DisplayName = "None"),

	Hand_Light  UMETA(DisplayName = "Right Hand Light"),
	Hand_EMF    UMETA(DisplayName = "Right Hand EMF"),
	Hand_Tool   UMETA(DisplayName = "Right Hand Tool"),
	Hand_Thermo UMETA(DisplayName = "Right Hand Thermo"),
	Head        UMETA(DisplayName = "Head")
};

UCLASS()
class PROJECTSENTINELLABS_API ASFW_EquippableBase
	: public AActor
	, public ISFW_InteractableInterface
{
	GENERATED_BODY()

public:
	ASFW_EquippableBase();

	virtual void BeginPlay() override;

protected:


	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Equippable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equippable")
	//TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equippable")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY()
	FTransform InitialPhysicsRelativeTransform;

	bool bHasCachedPhysicsRelativeTransform = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equippable")
	ESFWEquipSlot EquipSlot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equippable")
	FText DisplayName;

public:
	// ---------- Equip / unequip / drop ----------

	virtual void OnEquipped(ACharacter* NewOwnerChar);
	virtual void OnUnequipped();

	virtual void OnDropped(const FVector& DropLocation, const FVector& TossVelocity);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDropped(const FVector& DropLocation, const FVector& TossVelocity);

	// ---------- Placement API ----------

	/** Default: not placeable. Override in subclasses (REM-POD) to return true. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equippable|Placement")
	bool CanBePlaced() const;
	virtual bool CanBePlaced_Implementation() const { return false; }

	/** Base placement behavior. Called on all machines via Multicast_OnPlaced. */
	virtual void OnPlaced(const FTransform& WorldTransform);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnPlaced(const FTransform& WorldTransform);

	// ---------- Use ----------

	virtual void PrimaryUse() {}
	virtual void SecondaryUse() {}

	UStaticMeshComponent* GetMesh() const { return Mesh; }

	// Interactable interface
	virtual FText GetPromptText_Implementation() const override;
	virtual void Interact_Implementation(AController* InstigatorController) override;

	// For anim BP / equipment manager
	UFUNCTION(BlueprintPure, Category = "Equippable")
	ESFWEquipSlot GetEquipSlot() const { return EquipSlot; }

	UFUNCTION(BlueprintPure, Category = "Equippable")
	FName GetAttachSocketNameBP() const { return GetAttachSocketName(); }

protected:
	void AttachToCharacter(ACharacter* Char, FName Socket);
	void DetachFromCharacter();

	virtual UPrimitiveComponent* GetPhysicsComponent() const;
	virtual FName GetAttachSocketName() const;

	// Per-item offset relative to the hand socket
	UPROPERTY(EditDefaultsOnly, Category = "Equip")
	FTransform HandOffset = FTransform::Identity;
	
private: 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equippable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPrimitiveComponent> PhysicsComponent = nullptr;

public:
	// Voice comms flag
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Voice")
	bool GrantsRadioComms() const;
	virtual bool GrantsRadioComms_Implementation() const { return false; }

	// Anim-facing type
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equippable|Anim")
	EHeldItemType GetAnimHeldType() const;
	virtual EHeldItemType GetAnimHeldType_Implementation() const { return EHeldItemType::None; }
};
