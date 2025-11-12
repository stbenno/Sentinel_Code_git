// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/AnomalySystems/SFW_DecisionTypes.h"
#include "SFW_DoorBase.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;
class UPrimitiveComponent;
class APawn;
class USoundBase;
class ASFW_AnomalyDecisionSystem;
class ASFW_DoorScareFX;

UENUM(BlueprintType)
enum class EDoorState : uint8
{
	Closed  UMETA(DisplayName = "Closed"),
	Open    UMETA(DisplayName = "Open"),
	Opening UMETA(DisplayName = "Opening"),
	Closing UMETA(DisplayName = "Closing"),
};

UCLASS(Blueprintable)
class PROJECTSENTINELLABS_API ASFW_DoorBase : public AActor
{
	GENERATED_BODY()

public:
	ASFW_DoorBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Decision-system entry
	UFUNCTION(Server, Reliable)
	void HandleDecision(const FSFWDecisionPayload& Payload);

	// Manual controls
	UFUNCTION(BlueprintCallable, Category = "SFW|Door")
	void OpenDoor();

	UFUNCTION(BlueprintCallable, Category = "SFW|Door")
	void CloseDoor();

	UFUNCTION(BlueprintCallable, Category = "SFW|Door")
	void LockDoor(float Duration);

	UFUNCTION(BlueprintCallable, Category = "SFW|Door")
	void Unlock();

	UFUNCTION(BlueprintPure, Category = "SFW|Door")
	bool IsLocked() const;

	UFUNCTION(BlueprintPure, Category = "SFW|Door")
	FName GetRoomID() const { return RoomID; }

	// Debug: force scare
//	UFUNCTION(Exec, Server, Reliable)
//	void Server_Debug_ForceScare(APawn* InstigatorPawn);

protected:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* Frame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* Door;

	// Kept on the frame so it does not move with the leaf
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USphereComponent* ProximityTrigger;

	// Anchor used for FX spawn; lets you position easily in editor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* ScareFXAnchor;

	// Initial/random state
	UPROPERTY(EditAnywhere, Category = "SFW|Door|Initial")
	EDoorState InitialState = EDoorState::Closed;

	UPROPERTY(EditAnywhere, Category = "SFW|Door|Initial")
	bool bRandomizeInitialState = false;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRandomizeInitialState"), Category = "SFW|Door|Initial")
	float InitialOpenChance = 0.5f;

	// Motion config
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Door")
	float ClosedYaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Door")
	float OpenYaw = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Door")
	float AnimDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Door")
	FName RoomID = NAME_None;

	// --- Scare FX placement controls (restored) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFW|Scare")
	FVector ScareFXLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFW|Scare")
	FRotator ScareFXRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFW|Scare")
	bool bAttachFXToAnchor = false;

	// FX class and timing
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Scare")
	TSubclassOf<ASFW_DoorScareFX> ScareFXClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Scare")
	float ScareFXLifetime = 2.0f;

	// Delay from FX start to the actual slam (to sync to animation)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Scare")
	float SlamImpactDelay = 0.25f;

	// Replicated SFX at slam
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFW|Scare")
	USoundBase* SlamSFX = nullptr;

	// Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	EDoorState State = EDoorState::Closed;

	UFUNCTION()
	void OnRep_State();

	UPROPERTY(Replicated)
	float LockEndTime = 0.f;

	// Scare logic
	UPROPERTY(EditAnywhere, Category = "SFW|Scare")
	float ScareChance = 0.15f;

	UPROPERTY(EditAnywhere, Category = "SFW|Scare")
	float ScareLockDuration = 2.0f;

	UPROPERTY(EditAnywhere, Category = "SFW|Scare")
	float ScareCooldown = 10.0f;

	FTimerHandle Timer_SlamImpact;

	// Motion state
	float StartYaw = 0.f;
	float TargetYaw = 0.f;
	float AnimT = 0.f;

	// Per-door cooldown
	float LastScareTime = -1000.f;

	// Helpers
	void ApplyState();
	void FinishMotion();
	void SnapTo(float YawDeg);
	float GetYaw() const;

	void TryDoorScare(APawn* ApproachingPawn, bool bForce = false);
	void StartSlamSequence(APawn* ApproachingPawn);
	void OnSlamImpact();

	FTransform ComputeScareFXTransform() const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlaySlamFX(const FTransform& Where);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlaySlamSFX(const FVector& Loc);

	UFUNCTION()
	void OnProximityBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
