// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h"
#include "SFW_HandHeldItemDataAsset.generated.h"


class UAnimSequence;
/**
 * 
 */
UCLASS()
class PROJECTSENTINELLABS_API USFW_HandHeldItemDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EHeldItemType ItemType = EHeldItemType::None; 
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Animation")
	TObjectPtr<UAnimSequence> TP_HoldPose = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Attach")
	FName TP_AttachSocket = TEXT("hand_R_Tool");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Attach")
	FTransform TP_AttachOffset = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Behavior")
	bool bHasLight = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Behavior", meta=(EditCondition="bHasLight"))
	float LightIntensityTP = 2000.f;







};
