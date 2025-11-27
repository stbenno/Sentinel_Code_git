// SFW_AnomalyController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFW_DecisionTypes.h"        // ESFWAnomalyType, FSFWDecisionPayload
#include "SFW_AnomalyController.generated.h"

class ARoomVolume;
class ASFW_GameState;
class ASFW_ShadeCharacterBase;
class ASFW_DoorBase;

DECLARE_LOG_CATEGORY_EXTERN(LogAnomalyController, Log, All);

/** High-level Shade lifecycle (kept on the controller for now). */
UENUM(BlueprintType)
enum class EShadePhase : uint8
{
    Dormant      UMETA(DisplayName = "Dormant"),       // no Shade in world
    RoamingToRift UMETA(DisplayName = "RoamingToRift"), // spawned in Base, moving / patrolling toward Rift
    AtRift       UMETA(DisplayName = "AtRift"),        // has reached Rift room & is empowering it
    Hunting      UMETA(DisplayName = "Hunting")        // full hunt mode (not implemented yet)
};

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

    /** Query if we currently have an active Shade instance owned by this controller. */
    UFUNCTION(BlueprintPure, Category = "Anomaly|Shade")
    bool HasActiveShade() const;

    /** Current high-level Shade phase. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anomaly|Shade")
    EShadePhase ShadePhase = EShadePhase::Dormant;

    /** Handle Shade-related decisions coming from the decision system (server only). */
    void HandleShadeDecision(const FSFWDecisionPayload& Payload);

protected:
    virtual void BeginPlay() override;

    // ---- Room selection ----
    void PickRooms();

    /** Base room for the round (must be non-Safe / non-Hallway). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rooms")
    ARoomVolume* BaseRoom = nullptr;

    /** Rift / anomaly focus room (must be non-Safe / non-Hallway). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rooms")
    ARoomVolume* RiftRoom = nullptr;

    // ---- Shade support ----

    /** BP class for the Binder’s Shade. Set this in defaults to your BP_SFW_ShadeBase. */
    UPROPERTY(EditDefaultsOnly, Category = "Anomaly|Shade")
    TSubclassOf<ASFW_ShadeCharacterBase> ShadeClass;

    /** Runtime Shade instance for this round (at most one). */
    UPROPERTY()
    ASFW_ShadeCharacterBase* ActiveShade = nullptr;

    /** Spawns the Shade in/near BaseRoom on navmesh and opens Base doors so it can leave. */
    void SpawnInitialShade();

    

private:
    ASFW_GameState* GS() const;

    /** Excludes Hallway and Safe from any consideration. */
    bool IsForbiddenRoom(ARoomVolume* R) const;

    /** Decide which anomaly type this round uses (Binder, Splitter, etc). */
    void PickAnomalyType();
};
