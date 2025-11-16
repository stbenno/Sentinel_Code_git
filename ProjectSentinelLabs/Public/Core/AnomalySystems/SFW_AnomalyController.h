// SFW_AnomalyController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFW_DecisionTypes.h"        // ESFWAnomalyType
#include "SFW_AnomalyController.generated.h"

class ARoomVolume;
class ASFW_GameState;

DECLARE_LOG_CATEGORY_EXTERN(LogAnomalyController, Log, All);

/**
 * Picks Base / Rift rooms and marks round state.
 * Also chooses which anomaly archetype (Binder / Splitter / etc) this round uses.
 * All decision scheduling is handled by ASFW_AnomalyDecisionSystem.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_AnomalyController : public AActor
{
	GENERATED_BODY()

public:
	ASFW_AnomalyController();

	/** Entry point from game mode or level blueprint. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Anomaly")
	void StartRound();

	/** Which anomaly archetype is active this round. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly")
	ESFWAnomalyType ActiveAnomalyType = ESFWAnomalyType::Binder;

protected:
	virtual void BeginPlay() override;

	// ---- Room selection ----
	void PickRooms();

	/** Base room for the round (must be RoomType == Base). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rooms")
	ARoomVolume* BaseRoom = nullptr;

	/** Rift / anomaly focus room (must be RoomType == RiftCandidate). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rooms")
	ARoomVolume* RiftRoom = nullptr;

private:
	ASFW_GameState* GS() const;

	/** Excludes Hallway and Safe from any consideration. */
	bool IsForbiddenRoom(ARoomVolume* R) const;

	/** Decide which anomaly type this round uses (Binder, Splitter, etc). */
	void PickAnomalyType();
};
