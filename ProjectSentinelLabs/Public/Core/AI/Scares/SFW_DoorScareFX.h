// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFW_DoorScareFX.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;

/**
 * Transient scare visual for door slam.
 * Spawns on server. Replicates. Plays one montage. Dies after LifetimeSec.
 */
UCLASS(Blueprintable)
class PROJECTSENTINELLABS_API ASFW_DoorScareFX : public AActor
{
	GENERATED_BODY()

public:
	ASFW_DoorScareFX();

	/* Auto-destroy after this many seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	float Lifetime = 1.5f;

protected:
	virtual void BeginPlay() override;

	// Mesh for the shade / figure / hands etc
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FX")
	USkeletalMeshComponent* ScareMesh;

	// Optional montage to play once at spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	TObjectPtr<UAnimMontage> ScareMontage = nullptr;

	// Montage play rate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	float MontagePlayRate = 1.0f;

	// Lifetime in seconds before this actor auto-destroys
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	float LifetimeSec = 2.0f;
};
