// SFW_REMPod.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "SFW_REMPod.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class UPrimitiveComponent;

/**
 * Handheld or placeable REM-POD device.
 * PrimaryUse() toggles power. Placement is triggered via EquipmentManager::Server_PlaceActive().
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_REMPod : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_REMPod();

	// Equippable
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;
	virtual void PrimaryUse() override;

	// Anim type
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;

	// Placement opt-in
	virtual bool CanBePlaced_Implementation() const override { return true; }

	// Optional: customize placed behavior (calls Super then applies extras)
	virtual void OnPlaced(const FTransform& WorldTransform) override;

	// Power API
	UFUNCTION(BlueprintCallable, Category = "REM")
	void SetActive(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "REM")
	bool IsActive() const { return bIsActive; }

protected:
	// Visible / physical body
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "REM")
	TObjectPtr<UStaticMeshComponent> PodMesh;

	// Idle hum while active (3D)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "REM|Audio")
	TObjectPtr<UAudioComponent> HumAudioComp;

	// One-shots
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "REM|Audio")
	TObjectPtr<USoundBase> PowerOnSFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "REM|Audio")
	TObjectPtr<USoundBase> PowerOffSFX = nullptr;

	// Which component should get physics when dropped
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

	// Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_IsActive, BlueprintReadOnly, Category = "REM")
	bool bIsActive = false;

	UFUNCTION()
	void OnRep_IsActive();

	// Server authority for power
	UFUNCTION(Server, Reliable)
	void Server_SetActive(bool bEnable);

	// Apply visuals/audio for current state
	void ApplyActiveState();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
