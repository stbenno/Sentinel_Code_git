// SFW_TPAnimInstance.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h"
#include "SFW_TPAnimInstance.generated.h"

class USFW_EquipmentManagerComponent;

UCLASS()
class PROJECTSENTINELLABS_API USFW_TPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	USFW_TPAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	APawn* OwningPawn = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	USFW_EquipmentManagerComponent* EquipComp = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	EHeldItemType HeldItemType = EHeldItemType::None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	EEquipState EquipState = EEquipState::Idle;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	bool bIsHolding = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	bool bIsCrouched = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	float Speed = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	float Direction = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	bool bHasItemEquipped = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Anim")
	bool bIsEquipping = false;

	// For aim offset (pitch up/down in degrees, relative to actor forward)
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Aim")
	float AimPitch = 0.f;
};
