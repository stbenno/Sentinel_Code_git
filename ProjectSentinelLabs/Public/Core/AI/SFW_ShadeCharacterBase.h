// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SFW_ShadeCharacterBase.generated.h"

class USphereComponent;
class APawn;
class ARoomVolume;

UENUM(BlueprintType)
enum class EShadeState : uint8
{
    Idle,
    Patrol,
    Chase,
    Stunned
};

UCLASS()
class PROJECTSENTINELLABS_API ASFW_ShadeCharacterBase : public ACharacter
{
    GENERATED_BODY()

public:
    ASFW_ShadeCharacterBase();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 0..1 global aggression factor (can be fed from GameState / anomaly). */
    UFUNCTION(BlueprintCallable, Category = "Shade")
    void SetAggressionFactor(float InFactor);

    /** State machine entry. */
    void SetShadeState(EShadeState NewState);

    UFUNCTION(BlueprintPure, Category = "Shade")
    EShadeState GetShadeState() const { return ShadeState; }

    // ----- AI helpers: home + patrol + target -----

    /** Set where this Shade “belongs” (e.g. BaseRoom center). */
    UFUNCTION(BlueprintCallable, Category = "Shade|AI")
    void InitializeHome(const FVector& InHomeLocation);

    /** Current home location (used for patrol). */
    UFUNCTION(BlueprintPure, Category = "Shade|AI")
    FVector GetHomeLocation() const { return HomeLocation; }

    /** How far from HomeLocation this Shade should normally roam. */
    UFUNCTION(BlueprintPure, Category = "Shade|AI")
    float GetPatrolRadius() const { return PatrolRadius; }

    /** AI controller / systems can query if there’s an active target. */
    UFUNCTION(BlueprintPure, Category = "Shade|AI")
    bool HasTarget() const { return CurrentTarget != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Shade|AI")
    APawn* GetTarget() const { return CurrentTarget; }

    /** Explicitly set/clear target from BT or C++. */
    UFUNCTION(BlueprintCallable, Category = "Shade|AI")
    void SetTarget(APawn* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Shade|AI")
    void ClearTarget();

    /** Current room id the Shade is inside (server-authored, replicated). */
    UFUNCTION(BlueprintPure, Category = "Shade|AI")
    FName GetCurrentRoomId() const { return CurrentRoomId; }

protected:
    // --- Movement tuning ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float BaseWalkSpeed = 300.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float MaxAggroBonusSpeed = 300.f; // added when chasing

    // --- Replicated state ---
    UPROPERTY(ReplicatedUsing = OnRep_ShadeState, BlueprintReadOnly, Category = "State")
    EShadeState ShadeState = EShadeState::Idle;

    UFUNCTION()
    void OnRep_ShadeState();

    // cached 0..1
    UPROPERTY(Transient, BlueprintReadOnly, Category = "State")
    float AggressionFactor = 0.f;

    void ApplyMovementSpeed();

    // ----- Sensing / simple detection -----

    /** Sphere for cheap “I see a player” style detection. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shade|Sensing")
    USphereComponent* DetectionSphere;

    /** Radius for initial detection. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shade|Sensing")
    float DetectionRadius = 700.f;

    /** Extra buffer before we consider the target “lost”. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shade|Sensing")
    float LoseTargetDistance = 900.f;

    /** Where this Shade was “anchored” (e.g. BaseRoom center). */
    UPROPERTY(BlueprintReadOnly, Category = "Shade|AI")
    FVector HomeLocation = FVector::ZeroVector;

    /** How far from HomeLocation the Shade should patrol. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shade|AI")
    float PatrolRadius = 900.f;

    /** Server-side only target reference. */
    UPROPERTY(Transient)
    APawn* CurrentTarget = nullptr;

    // Detection callbacks
    UFUNCTION()
    void OnDetectionBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnDetectionEnd(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex);

    // ----- Room tracking -----

    /** Replicated id of the room volume the Shade is currently in (or None). */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentRoomId, BlueprintReadOnly, Category = "Shade|AI")
    FName CurrentRoomId = NAME_None;

    /** Non-replicated pointer to the current room volume. */
    UPROPERTY(Transient)
    ARoomVolume* CurrentRoomVolume = nullptr;

    UFUNCTION()
    void OnRep_CurrentRoomId();

    /** Server-only: set the current room (updates pointer + id). */
    void SetCurrentRoom(ARoomVolume* NewRoom);

    /** Server-only: scan world once to find which room we start in. */
    void RefreshCurrentRoomFromWorld();

    /** Called when the Shade begins overlapping another actor (we watch for ARoomVolume). */
    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

    /** Called when the Shade stops overlapping another actor. */
    virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
};
