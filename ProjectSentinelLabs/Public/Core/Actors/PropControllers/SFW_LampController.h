// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

/*#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Core/AnomalySystems/SFW_DecisionTypes.h"
#include "SFW_LampController.generated.h"

UENUM(BlueprintType)
enum class ESFWLampModes : uint8 { Off, On, Flicker, NoPower };

UCLASS(ClassGroup = (SFW), meta = (BlueprintSpawnableComponent))
class PROJECTSENTINELLABS_API USFW_LampController : public UActorComponent
{
	GENERATED_BODY()

public:
	USFW_LampController();

	// Set by lamp BP at BeginPlay from its BP_RoomVolume
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp") FName OwningRoomId = NAME_None;

	// Player toggle
	UFUNCTION(Server, Reliable) void ServerPlayerToggle();

	// External inputs
	void OnPowerChanged(bool bPowered);                          // site/room power
	UFUNCTION() void HandleDecisionBP(const FSFWDecisionPayload& P); // bind to DecisionSystem

	// Current resolved mode for clients (rep)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mode) ESFWLampMode Mode = ESFWLampMode::Off;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// BP visual hook
	UFUNCTION(BlueprintImplementableEvent) void BP_ApplyLampMode(ESFWLampMode NewMode, float FlickerDuration);

private:
	// State inputs (rep on server)
	UPROPERTY(Replicated) bool bHasPower = true;
	UPROPERTY(Replicated) bool bDesiredOn = false;
	UPROPERTY(Replicated) float FlickerEndTime = 0.f; // server time

	UFUNCTION() void OnRep_Mode();

	void RecomputeMode(); // server computes Mode from inputs
	void ApplyMode(bool bFromRep); // calls BP for visuals


}; */