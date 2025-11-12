// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/AnomalySystems/SFW_DecisionTypes.h"
#include "SFW_LampComp.generated.h"

UENUM(BlueprintType)
enum class ESFWLampMode : uint8 { Off, On, Flicker, NoPower };

UCLASS(ClassGroup = (SFW), Blueprintable, meta = (BlueprintSpawnableComponent))
class PROJECTSENTINELLABS_API USFW_LampComp : public UActorComponent
{
	GENERATED_BODY()
public:
	USFW_LampComp();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	FName OwningRoomId = NAME_None;

	UFUNCTION(Server, Reliable)
	void ServerPlayerToggle();

	void OnPowerChanged(bool bPowered);

	UFUNCTION()
	void HandleDecisionBP(const FSFWDecisionPayload& P);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mode)
	ESFWLampMode Mode = ESFWLampMode::Off;

	// BP visual hook
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Apply Lamp Mode")
	void BP_ApplyLampMode(ESFWLampMode NewMode, float FlickerDuration);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	bool bHasPower = true;

	UPROPERTY(Replicated)
	bool bDesiredOn = false;

	UPROPERTY(Replicated)
	float FlickerEndSec = 0.f;

	UFUNCTION()
	void OnRep_Mode();

	// server only
	void RecomputeMode();

	// client + server
	void ApplyMode(bool FromRep);
};
