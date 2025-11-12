// SFW_SigilActor.cpp

#include "Core/AnomalySystems/SFW_SigilActor.h"

#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

ASFW_SigilActor::ASFW_SigilActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SigilDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("SigilDecal"));
	SigilDecal->SetupAttachment(RootComponent);

	// Small default projection box; you rotate the actor in the editor for floor/wall
	SigilDecal->DecalSize = FVector(8.f, 64.f, 64.f);
	SigilDecal->SetVisibility(true);

	// Default UV charge timings (can be overridden in BP)
	UVChargeSeconds = 3.f;
	UVChargeResetSeconds = 0.5f;

	UVAccumulatedTime = 0.f;
	LastUVHitTime = 0.f;
	bHasBeenFullyRevealed = false;
}

void ASFW_SigilActor::BeginPlay()
{
	Super::BeginPlay();

	if (SigilDecal)
	{
		// Create a dynamic material instance from element 0
		SigilMID = SigilDecal->CreateDynamicMaterialInstance();

		// start invisible
		if (SigilMID)
		{
			SigilMID->SetScalarParameterValue(OpacityParamName, 0.f);
		}
	}

	bIsRevealed = false;
	RevealTimeRemaining = 0.f;
	UVAccumulatedTime = 0.f;
	LastUVHitTime = 0.f;
	bHasBeenFullyRevealed = false;
}

void ASFW_SigilActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateVisual(DeltaTime);
}

void ASFW_SigilActor::ActivateAsReal()
{
	bIsActive = true;
	bIsReal = true;

	bIsRevealed = false;
	RevealTimeRemaining = 0.f;

	UVAccumulatedTime = 0.f;
	LastUVHitTime = 0.f;
	bHasBeenFullyRevealed = false;

	if (SigilMID)
	{
		SigilMID->SetScalarParameterValue(OpacityParamName, 0.f);
	}
}

void ASFW_SigilActor::ActivateAsFake()
{
	bIsActive = true;
	bIsReal = false;

	bIsRevealed = false;
	RevealTimeRemaining = 0.f;

	UVAccumulatedTime = 0.f;
	LastUVHitTime = 0.f;
	bHasBeenFullyRevealed = false;

	if (SigilMID)
	{
		SigilMID->SetScalarParameterValue(OpacityParamName, 0.f);
	}
}

void ASFW_SigilActor::ResetSigil()
{
	bIsActive = false;
	bIsReal = false;
	bIsRevealed = false;
	RevealTimeRemaining = 0.f;

	UVAccumulatedTime = 0.f;
	LastUVHitTime = 0.f;
	bHasBeenFullyRevealed = false;

	if (SigilMID)
	{
		SigilMID->SetScalarParameterValue(OpacityParamName, 0.f);
	}
}

void ASFW_SigilActor::ExposeByUV(float ExtraSeconds)
{
	// Expect to be called on server
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsActive)
	{
		// Not part of this round's layout
		return;
	}

	const float Clamped = FMath::Clamp(ExtraSeconds, 0.f, MaxVisibleSeconds);

	// Extend time if this is longer than current remaining
	RevealTimeRemaining = FMath::Max(RevealTimeRemaining, Clamped);
	bIsRevealed = (RevealTimeRemaining > 0.f);

	// Immediate visual bump on server
	UpdateVisual(0.f);

	// Tell all clients
	Multicast_ExposeByUV(RevealTimeRemaining);
}

void ASFW_SigilActor::NotifyUVHit(float DeltaSeconds)
{
	// Server-only, and only for active sigils
	if (!HasAuthority() || !bIsActive)
	{
		return;
	}

	// If already fully revealed, ignore further charge
	if (bHasBeenFullyRevealed)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// If there was a big gap since last UV hit, reset progress
	if (LastUVHitTime > 0.f &&
		(Now - LastUVHitTime) > UVChargeResetSeconds)
	{
		UVAccumulatedTime = 0.f;
	}

	UVAccumulatedTime += DeltaSeconds;
	LastUVHitTime = Now;

	// Fully charged?
	if (UVAccumulatedTime >= UVChargeSeconds)
	{
		bHasBeenFullyRevealed = true;

		// Make it visible for everyone for the full puzzle window
		ExposeByUV(MaxVisibleSeconds);
	}
}

void ASFW_SigilActor::Multicast_ExposeByUV_Implementation(float Duration)
{
	RevealTimeRemaining = Duration;
	bIsRevealed = (RevealTimeRemaining > 0.f);
	UpdateVisual(0.f);
}

void ASFW_SigilActor::UpdateVisual(float DeltaTime)
{
	if (RevealTimeRemaining > 0.f && DeltaTime > 0.f)
	{
		RevealTimeRemaining = FMath::Max(0.f, RevealTimeRemaining - DeltaTime);
		if (RevealTimeRemaining <= 0.f)
		{
			bIsRevealed = false;
		}
	}

	float Alpha = 0.f;

	if (bIsRevealed && RevealTimeRemaining > 0.f)
	{
		if (FadeOutSeconds <= 0.f || RevealTimeRemaining > FadeOutSeconds)
		{
			// fully visible
			Alpha = 1.f;
		}
		else
		{
			// fade out over last FadeOutSeconds
			Alpha = FMath::Clamp(RevealTimeRemaining / FadeOutSeconds, 0.f, 1.f);
		}
	}
	else
	{
		Alpha = 0.f;
	}

	if (SigilMID)
	{
		SigilMID->SetScalarParameterValue(OpacityParamName, Alpha);
	}
}
