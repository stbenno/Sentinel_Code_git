#pragma once

#include "Engine/DataTable.h"
#include "SFW_DecisionTypes.generated.h"


// ------------------------------------------------------
// Anomaly "truth" types (one per ghost / entity variant)
// ------------------------------------------------------
UENUM(BlueprintType)
enum class ESFWAnomalyType : uint8
{
    None     UMETA(DisplayName = "None"),
    Binder   UMETA(DisplayName = "Binder"),
    Watcher  UMETA(DisplayName = "Watcher"),
    Chill    UMETA(DisplayName = "Chill"),
    Parasite UMETA(DisplayName = "Parasite"),
    Splitter UMETA(DisplayName = "Splitter")
    // Add more types here later:
    // Whisperer
};


// ------------------------------------------------------
// Per-anomaly profile row (used by a DataTable)
// "What evidence belongs to this type?"
// ------------------------------------------------------
USTRUCT(BlueprintType)
struct FSFWAnomalyProfileRow : public FTableRowBase
{
    GENERATED_BODY()

    // Which anomaly this row describes
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESFWAnomalyType AnomalyType = ESFWAnomalyType::None;

    // Nice name for UI, binder, etc
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    // Evidence that counts as Tier 1 for this anomaly
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> Tier1EvidenceIds;

    // Evidence that counts as Tier 2 for this anomaly
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> Tier2EvidenceIds;

    // Non-tier “characteristic” clues
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> CharacteristicIds;

    // ---- Behavior biases for the decision system ----
    // Multiplier for door-related decisions (Open/Close/Lock/etc)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float DoorBias = 1.0f;

    // Multiplier for light-related decisions (LampFlicker/BlackoutRoom)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float LightBias = 1.0f;
};


// ------------------------------------------------------
// Decision table rows (what the system can choose to do)
// ------------------------------------------------------
UENUM(BlueprintType)
enum class ESFWDecision : uint8
{
    Idle,
    LampFlicker,
    LockDoor,
    JamDoor,
    OpenDoor,
    CloseDoor,
    KnockDoor,
    JumpScare,
    Teleport,
    Trap,
    Patrol,
    Hunt,
    EvidenceT1,
    EvidenceT2,
    Characteristic,
    BlackoutRoom,
    PropPulse,
    PropToss

};

USTRUCT(BlueprintType)
struct FSFWDecisionRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32        Tier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESFWDecision Type = ESFWDecision::Idle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float        Weight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float        CooldownSec = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float        Magnitude = 1.f; // use for intensity if needed
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float        Duration = 2.f;  // seconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName        TargetRoomId = NAME_None; // optional; leave None to use fallback
};

USTRUCT(BlueprintType)
struct FSFWDecisionPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) ESFWDecision Type = ESFWDecision::Idle;
    UPROPERTY(BlueprintReadWrite) FName        RoomId = NAME_None;
    UPROPERTY(BlueprintReadWrite) float        Magnitude = 1.f;
    UPROPERTY(BlueprintReadWrite) float        Duration = 2.f;

    // Weak on purpose; payloads are transient and should not pin actors for GC
    UPROPERTY() TWeakObjectPtr<AActor> Instigator = nullptr; // replaces raw AActor*
};
