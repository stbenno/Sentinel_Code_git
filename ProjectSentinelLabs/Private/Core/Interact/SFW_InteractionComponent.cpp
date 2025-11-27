// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Interact/SFW_InteractionComponent.h"

#include "Core/Actors/Interface/SFW_InteractableInterface.h"
#include "core/Actors/SFW_EquippableBase.h"
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

	FVector EyesLoc;
	FRotator EyesRot;
	PC->GetPlayerViewPoint(EyesLoc, EyesRot);

	const FVector Start = EyesLoc;
	const FVector End = Start + EyesRot.Vector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractTrace), false, GetOwner());
	FHitResult Hit;

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Cyan : FColor::Red,
			false, 0.12f, 0, 0.8f);
	}

	if (!bHit)
	{
		if (bDrawDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("[InteractTrace] No hit"));
		}
		return false;
	}

	AActor* HitActor = Hit.GetActor();
	UPrimitiveComponent* HitComp = Hit.Component.Get();

	const bool bIsEquippable = HitActor && HitActor->IsA(ASFW_EquippableBase::StaticClass());
	const bool bImplementsInterface =
		HitActor && HitActor->GetClass()->ImplementsInterface(USFW_InteractableInterface::StaticClass());

	if (bDrawDebug)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[InteractTrace] HitActor=%s Comp=%s ObjType=%d Equippable=%d ImplementsInterface=%d Dist=%.1f"),
			HitActor ? *HitActor->GetName() : TEXT("None"),
			HitComp ? *HitComp->GetName() : TEXT("None"),
			HitComp ? (int32)HitComp->GetCollisionObjectType() : -1,
			bIsEquippable ? 1 : 0,
			bImplementsInterface ? 1 : 0,
			FVector::Dist(EyesLoc, Hit.ImpactPoint));

		DrawDebugSphere(GetWorld(), Hit.ImpactPoint,
			6.f, 12,
			bImplementsInterface ? FColor::Green : FColor::Red,
			false, 0.12f);
	}

	if (!HitActor) return false;
	if (!bImplementsInterface) return false;

	const float Dist = FVector::Dist(EyesLoc, Hit.ImpactPoint);
	if (Dist > InteractRange + 2.f) return false;

	OutActor = HitActor;
	return true;
}

void USFW_InteractionComponent::OnInteractInput()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	if (!FocusedActor) return;

	UE_LOG(LogTemp, Log, TEXT("[Interact] %s pressed Interact; FocusedActor=%s"),
		*OwnerPawn->GetName(),
		*FocusedActor->GetName());

	// Always go through the server RPC (on listen/server this just calls _Implementation)
	Server_TryInteract(FocusedActor);
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

	// Rebuild the same view trace on the server
	FVector EyesLoc;
	FRotator EyesRot;
	Pawn->GetActorEyesViewPoint(EyesLoc, EyesRot);

	const FVector Start = EyesLoc;
	const FVector End = Start + EyesRot.Vector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ServerInteractTrace), false, Pawn);
	FHitResult Hit;

	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false; // nothing in front of the player on the server
	}

	AActor* HitActor = Hit.GetActor();
	if (HitActor != Target)
	{
		// Client says “interact with X” but server trace sees something else.
		return false;
	}

	const float Dist = FVector::Dist(EyesLoc, Hit.ImpactPoint);
	if (Dist > InteractRange + 2.f)
	{
		return false;
	}

	if (!Target->GetClass()->ImplementsInterface(USFW_InteractableInterface::StaticClass()))
	{
		return false;
	}

	return true;
}
