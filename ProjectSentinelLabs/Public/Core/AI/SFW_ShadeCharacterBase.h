// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SFW_ShadeCharacterBase.generated.h"

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

    UFUNCTION(BlueprintCallable, Category = "Shade")
    void SetAggressionFactor(float InFactor);               // 0..1

    void SetShadeState(EShadeState NewState);
    UFUNCTION(BlueprintPure, Category = "Shade")
    EShadeState GetShadeState() const { return ShadeState; }

protected:
    // --- Movement tuning
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float BaseWalkSpeed = 300.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float MaxAggroBonusSpeed = 300.f; // added when chasing

    // --- Replicated state
    UPROPERTY(ReplicatedUsing = OnRep_ShadeState, BlueprintReadOnly, Category = "State")
    EShadeState ShadeState = EShadeState::Idle;

    UFUNCTION()
    void OnRep_ShadeState();

    // cached 0..1
    UPROPERTY(Transient, BlueprintReadOnly, Category = "State")
    float AggressionFactor = 0.f;

    void ApplyMovementSpeed();
};

