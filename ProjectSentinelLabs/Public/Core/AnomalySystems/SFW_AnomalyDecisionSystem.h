#pragma once

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Core/AnomalySystems/SFW_DecisionTypes.h"
#include "Core/Lights/SFW_PowerLibrary.h"
#include "SFW_AnomalyDecisionSystem.generated.h"   // <-- NOTE: no AnomalyProp include here

class ARoomVolume;
class ASFW_PlayerState;


DECLARE_MULTICAST_DELEGATE_OneParam(FSFWOnDecision, const FSFWDecisionPayload&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFWOnDecisionBP, const FSFWDecisionPayload&, Payload);

/**
 * Single source of truth for anomaly decisions.
 * - Reads FSFWDecisionRow from a DataTable.
 * - Chooses room based on player occupancy / sanity tier.
 * - Broadcasts FSFWDecisionPayload to C++ and BP listeners.
 */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_AnomalyDecisionSystem : public AActor
{
	GENERATED_BODY()

public:
	ASFW_AnomalyDecisionSystem();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Fired on server only, for C++ listeners (doors, etc). */
	FSFWOnDecision OnDecision;

	/** Replicated via MulticastDecision to clients, for BP listeners (lamps, FX, etc). */
	UPROPERTY(BlueprintAssignable, Category = "Anomaly")
	FSFWOnDecisionBP OnDecisionBP;

protected:
	/** DataTable of FSFWDecisionRow. This is the brain’s config. */
	UPROPERTY(EditAnywhere, Category = "Anomaly")
	UDataTable* DecisionsDT = nullptr;

	/** Profiles for Binder / Watcher / etc (FSFWAnomalyProfileRow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly")
	UDataTable* AnomalyProfilesDT = nullptr;

	/** Which anomaly is currently active. For now set this by hand (eg, Binder). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly")
	ESFWAnomalyType ActiveAnomalyType = ESFWAnomalyType::Binder;

	/** How often to attempt a decision, in seconds (server only). */
	UPROPERTY(EditAnywhere, Category = "Anomaly")
	float IntervalSec = 2.0f;

	FTimerHandle TickHandle;

	/** Per-decision-type cooldown: ESFWDecision -> next allowed world time. */
	TMap<ESFWDecision, double> NextAllowedTime;

	// Cached behavior bias from the active profile
	float CachedDoorBias = 1.f;
	float CachedLightBias = 1.f;

	// Core loop
	UFUNCTION()
	void TickDecision();

	// Helpers
	bool IsReady(const FSFWDecisionRow& R) const;
	void StartCooldown(const FSFWDecisionRow& R);
	const FSFWDecisionRow* PickWeighted(int32 Tier);
	void Dispatch(const FSFWDecisionRow& R, FName RoomId);
	int32 GetRoomTier(FName RoomId) const;
	void RefreshBiasFromProfile();
	void HandlePropPulse(const FSFWDecisionRow& R, FName RoomId);

	/** Multicast payload to all clients, then raise OnDecisionBP. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDecision(const FSFWDecisionPayload& Payload);
};
