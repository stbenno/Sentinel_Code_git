// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SFW_AgentCatalog.generated.h"

// Forward declares to keep includes light
class UTexture2D;
class USkeletalMesh;
class UAnimInstance;

USTRUCT(BlueprintType)
struct FSFW_AgentEntry
{
	GENERATED_BODY()

	/** Stable ID (e.g., Sentinel.AgentA, Sentinel.AgentB) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AgentID = "Sentinel.AgentA";

	/** Display name for UI (e.g., "Agent A") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	/** UI portrait/icon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Third-person / world body mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	// --- NEW: first-person arms support ---
	/** First-person arms mesh (Only Owner See component) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FirstPerson")
	TObjectPtr<USkeletalMesh> ArmsMesh = nullptr;

	// Third-person hair mesh (modular piece driven by body anims) owner no see
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<USkeletalMesh> HairMesh = nullptr;

	/** Optional Anim BP class for the first-person arms */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FirstPerson")
	TSubclassOf<UAnimInstance> ArmsAnimClass = nullptr;
	// --- NEW END ---
};

UCLASS(BlueprintType)
class USFW_AgentCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Ordered list of available agents (drives left/right cycling order) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSFW_AgentEntry> Agents;
};
