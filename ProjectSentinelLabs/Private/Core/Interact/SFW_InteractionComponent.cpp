// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Interact/SFW_InteractionComponent.h"

#include "Core/Actors/Interface/SFW_InteractableInterface.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

USFW_InteractionComponent::USFW_InteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false); // client-only logic; RPC handles server
}

void USFW_InteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USFW_InteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only the owning client updates focus
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;

	TraceAccumulator += DeltaTime;
	const float Interval = (TraceRateHz > 0.f) ? (1.f / TraceRateHz) : 0.1f;
	if (TraceAccumulator < Interval) return;
	TraceAccumulator = 0.f;

	UpdateFocus();
}

void USFW_InteractionComponent::TryInteract()
{
	OnInteractInput();          // uses your focus/trace + Server_TryInteract
}

void USFW_InteractionComponent::TryUseOrInteract()
{
	OnInteractInput();          // later you can branch to a “Use” path
}

APlayerController* USFW_InteractionComponent::GetOwningPC() const
{
	if (APawn* P = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(P->GetController());
	}
	return nullptr;
}

void USFW_InteractionComponent::UpdateFocus()
{
	AActor* NewFocus = nullptr;
	if (FindBestInteractable(NewFocus))
	{
		SetFocus(NewFocus);
	}
	else
	{
		SetFocus(nullptr);
	}
}

void USFW_InteractionComponent::SetFocus(AActor* NewFocus)
{
	if (FocusedActor == NewFocus) return;
	FocusedActor = NewFocus;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebug)
	{
		if (FocusedActor)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[Interact] Focus -> %s"), *FocusedActor->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[Interact] Focus cleared"));
		}
	}
#endif
}

bool USFW_InteractionComponent::FindBestInteractable(AActor*& OutActor) const
{
	OutActor = nullptr;

	APlayerController* PC = GetOwningPC();
	if (!PC) return false;

	FVector EyesLoc; FRotator EyesRot;
	PC->GetPlayerViewPoint(EyesLoc, EyesRot);

	const FVector Start = EyesLoc;
	const FVector End = Start + EyesRot.Vector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractTrace), false, GetOwner());
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (bDrawDebug) DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.12f, 0, 0.5f);
		return false;
	}

	AActor* HitActor = Hit.GetActor();
	if (bDrawDebug) DrawDebugLine(GetWorld(), Start, Hit.ImpactPoint, FColor::Cyan, false, 0.12f, 0, 0.8f);

	if (!HitActor) return false;
	if (!HitActor->GetClass()->ImplementsInterface(USFW_InteractableInterface::StaticClass())) return false;

	// Range check (safety; the line already clamps range)
	const float Dist = FVector::Dist(EyesLoc, Hit.ImpactPoint);
	if (Dist > InteractRange + 2.f) return false;

	OutActor = HitActor;
	return true;
}

void USFW_InteractionComponent::OnInteractInput()
{
	// Client entry. Use current focus.
	if (!FocusedActor) return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	// Server performs the interaction
	if (OwnerPawn->HasAuthority())
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			if (ValidateServerInteract(FocusedActor, PC))
			{
				ISFW_InteractableInterface::Execute_Interact(FocusedActor, PC);
			}
		}
	}
	else
	{
		Server_TryInteract(FocusedActor);
	}
}

void USFW_InteractionComponent::Server_TryInteract_Implementation(AActor* Target)
{
	if (!Target) return;

	UWorld* W = GetWorld();
	if (!W) return;

	const double Now = W->TimeSeconds;
	if (LastServerInteractTime >= 0.0 && (Now - LastServerInteractTime) < DebounceSeconds)
	{
		return; // spam guard
	}
	LastServerInteractTime = Now;

	APlayerController* PC = GetOwningPC();
	if (!PC) return;

	if (!ValidateServerInteract(Target, PC)) return;

	ISFW_InteractableInterface::Execute_Interact(Target, PC);
}

bool USFW_InteractionComponent::ValidateServerInteract(AActor* Target, APlayerController* PC) const
{
	if (!Target || !PC) return false;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return false;

	// 1) Distance gate (use pawn location to target location)
	const float Dist = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (Dist > (InteractRange + 30.f)) return false;

	// 2) Line-of-sight gate
	if (!PC->LineOfSightTo(Target)) return false;

	// 3) Interface check
	if (!Target->GetClass()->ImplementsInterface(USFW_InteractableInterface::StaticClass())) return false;

	return true;
}