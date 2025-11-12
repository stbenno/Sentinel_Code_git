// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFW_LampBase.generated.h"

class UStaticMeshComponent;
class USFW_LampControllerComponent;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_LampBase : public AActor
{
	GENERATED_BODY()
public:
	ASFW_LampBase();

	/** Logical room this lamp belongs to. If None, auto-filled from overlapping RoomVolume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	FName RoomId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lamp")
	UStaticMeshComponent* Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lamp")
	USFW_LampControllerComponent* Lamp = nullptr;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
