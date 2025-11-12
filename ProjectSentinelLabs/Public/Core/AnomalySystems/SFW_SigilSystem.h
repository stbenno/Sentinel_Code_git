// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFW_SigilSystem.generated.h"

class ASFW_SigilActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSigilPuzzleComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSigilPuzzleFailed);

/**
 * Generic sigil puzzle manager.
 * - Finds all ASFW_SigilActor instances in the level.
 * - For a given RoomId, chooses a layout in that room (4 active, 3 real by default).
 * - Tracks cleanses and broadcasts success/fail.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_SigilSystem : public AActor
{
	GENERATED_BODY()

public:
	ASFW_SigilSystem();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** How many sigils are visible/active in the current run. Binder uses 4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sigil")
	int32 RequiredVisibleSigils = 4;

	/** How many of the visible sigils are "real". Binder uses 3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sigil")
	int32 RequiredRealSigils = 3;

	/**
	 * Build a layout for a specific room.
	 * - RoomId should match ARoomVolume::RoomId where the rift is.
	 * - Only sigils overlapping that room volume are considered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sigil")
	void InitializeLayoutForRoom(FName RoomId);

	/** Full reset (end of mission / re-roll). Clears active/real state and counters. */
	UFUNCTION(BlueprintCallable, Category = "Sigil")
	void ResetPuzzle();

	/**
	 * Notify the system that a sigil was cleansed.
	 * - Sigil = which one
	 * - bWasReal = whether it was known to be real at cleanse time
	 */
	UFUNCTION(BlueprintCallable, Category = "Sigil")
	void NotifySigilCleansed(ASFW_SigilActor* Sigil, bool bWasReal);

	/** Fired when all real sigils have been cleansed (success). */
	UPROPERTY(BlueprintAssignable, Category = "Sigil")
	FOnSigilPuzzleComplete OnPuzzleComplete;

	/** Fired when a fake sigil was cleansed (failure). */
	UPROPERTY(BlueprintAssignable, Category = "Sigil")
	FOnSigilPuzzleFailed OnPuzzleFailed;

	UFUNCTION(BlueprintPure, Category = "Sigil")
	bool IsPuzzleActive() const { return bPuzzleActive; }

	UFUNCTION(BlueprintPure, Category = "Sigil")
	bool IsPuzzleComplete() const { return bPuzzleComplete; }

protected:
	/** True while a layout is active for the current run. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sigil")
	bool bPuzzleActive = false;

	/** True once all real sigils are cleansed. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sigil")
	bool bPuzzleComplete = false;

	/** How many real sigils have been successfully cleansed. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sigil")
	int32 NumRealCleansed = 0;

	/** All sigils in the level (non-replicated, server-only list). */
	UPROPERTY()
	TArray<TWeakObjectPtr<ASFW_SigilActor>> AllSigils;

	/** Sigils that are active for this run's layout (e.g. 4 for Binder). */
	UPROPERTY()
	TArray<TWeakObjectPtr<ASFW_SigilActor>> ActiveSigils;

	/** Gather all ASFW_SigilActor instances in the world (server only). */
	void GatherAllSigils();

	/** Collect sigils that overlap a given room volume (by RoomId). */
	void GatherSigilsInRoom(FName RoomId, TArray<ASFW_SigilActor*>& OutSigils);

	/** Helper: choose RequiredVisibleSigils from Pool, mark RequiredRealSigils as real. */
	void BuildRandomLayoutFromPool(const TArray<ASFW_SigilActor*>& Pool);
};
