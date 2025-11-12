// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/AI/Scares/SFW_DoorScareFX.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

ASFW_DoorScareFX::ASFW_DoorScareFX()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);

	ScareMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ScareMesh"));
	SetRootComponent(ScareMesh);

	if (ScareMesh)
	{
		ScareMesh->SetIsReplicated(true);
		ScareMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASFW_DoorScareFX::BeginPlay()
{
	Super::BeginPlay();

	// play montage on spawn if provided
	if (ScareMontage && ScareMesh)
	{
		if (UAnimInstance* AnimInst = ScareMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(ScareMontage, MontagePlayRate);
		}
	}

	if (Lifetime > 0.f) SetLifeSpan(Lifetime);
}
