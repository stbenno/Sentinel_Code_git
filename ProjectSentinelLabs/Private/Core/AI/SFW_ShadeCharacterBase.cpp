// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/AI/SFW_ShadeCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ASFW_ShadeCharacterBase::ASFW_ShadeCharacterBase()
{
    PrimaryActorTick.bCanEverTick = false;

    SetReplicates(true);
    SetReplicateMovement(true);

    // Let AI possess when placed/spawned
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Initial speed
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void ASFW_ShadeCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    ApplyMovementSpeed();
}

void ASFW_ShadeCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFW_ShadeCharacterBase, ShadeState);
}

void ASFW_ShadeCharacterBase::SetAggressionFactor(float InFactor)
{
    AggressionFactor = FMath::Clamp(InFactor, 0.f, 1.f);
    ApplyMovementSpeed();
}

void ASFW_ShadeCharacterBase::SetShadeState(EShadeState NewState)
{
    if (ShadeState == NewState) return;
    ShadeState = NewState;
    ApplyMovementSpeed();
}

void ASFW_ShadeCharacterBase::OnRep_ShadeState()
{
    ApplyMovementSpeed();
}

void ASFW_ShadeCharacterBase::ApplyMovementSpeed()
{
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        const bool bChasing = (ShadeState == EShadeState::Chase);
        const float Target = BaseWalkSpeed + (bChasing ? (AggressionFactor * MaxAggroBonusSpeed) : 0.f);
        Move->MaxWalkSpeed = Target;
    }
}

