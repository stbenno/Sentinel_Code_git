// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/Match/SFW_ExtractionVolume.h"
#include "Core/Game/SFW_GameMode.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Core/Game/SFW_GameState.h"

ASFW_ExtractionVolume::ASFW_ExtractionVolume()
{
	//bCanEverTick = false;
}

void ASFW_ExtractionVolume::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap events (server authority will drive the logic).
	OnActorBeginOverlap.AddDynamic(this, &ASFW_ExtractionVolume::OnBoxBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &ASFW_ExtractionVolume::OnBoxEndOverlap);
}

void ASFW_ExtractionVolume::OnBoxBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	ControllersInside.Add(PC);
	ReevaluateExtractionState();
}

void ASFW_ExtractionVolume::OnBoxEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	ControllersInside.Remove(PC);
	ReevaluateExtractionState();
}

void ASFW_ExtractionVolume::ReevaluateExtractionState()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bAllInside = AreAllLivingPlayersInside();

	if (bAllInside && !bCountdownActive)
	{
		StartExtractionCountdown();
	}
	else if (!bAllInside && bCountdownActive)
	{
		CancelExtractionCountdown();
	}
}

bool ASFW_ExtractionVolume::AreAllLivingPlayersInside() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	int32 LivingCount = 0;
	int32 InsideCount = 0;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		APawn* Pawn = PC->GetPawn();
		if (!Pawn)
		{
			// Treat players without a pawn as "dead" or not participating.
			continue;
		}

		++LivingCount;

		if (ControllersInside.Contains(PC))
		{
			++InsideCount;
		}
	}

	return (LivingCount > 0 && InsideCount == LivingCount);
}

int32 ASFW_ExtractionVolume::GetLivingPlayerCount() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 LivingCount = 0;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		if (APawn* Pawn = PC->GetPawn())
		{
			++LivingCount;
		}
	}

	return LivingCount;
}

void ASFW_ExtractionVolume::StartExtractionCountdown()
{
	if (bCountdownActive) return;
	bCountdownActive = true;

	UWorld* World = GetWorld();
	if (ASFW_GameState* GS = World ? World->GetGameState<ASFW_GameState>() : nullptr)
	{
		GS->bExtractionCountdownActive = true;
		GS->ExtractionEndTime = World->GetTimeSeconds() + ExtractionTime;
	}

	GetWorldTimerManager().SetTimer(
		ExtractionTimerHandle,
		this,
		&ASFW_ExtractionVolume::HandleExtractionComplete,
		ExtractionTime,
		false);
}


void ASFW_ExtractionVolume::CancelExtractionCountdown()
{
	if (!bCountdownActive) return;
	bCountdownActive = false;

	if (ASFW_GameState* GS = GetWorld()->GetGameState<ASFW_GameState>())
	{
		GS->bExtractionCountdownActive = false;
		GS->ExtractionEndTime = 0.f;
	}

	GetWorldTimerManager().ClearTimer(ExtractionTimerHandle);
}

void ASFW_ExtractionVolume::HandleExtractionComplete()
{
	if (!HasAuthority())
	{
		return;
	}

	bCountdownActive = false;

	// Double-check that conditions are still valid (nobody stepped out at the last frame).
	if (!AreAllLivingPlayersInside())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExtractionVolume: Timer finished but not all players are inside anymore. Aborting."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ExtractionVolume: Extraction complete. Ending round as SUCCESS."));

	if (UWorld* World = GetWorld())
	{
		if (ASFW_GameMode* GM = World->GetAuthGameMode<ASFW_GameMode>())
		{
			// This will eventually drive ReturnToLobby via your existing EndRound/ReturnToLobby flow.
			GM->EndRound(true);
		}
		if (ASFW_GameState* GS = World->GetGameState<ASFW_GameState>())
		{
			GS->bExtractionCountdownActive = false;
			GS->ExtractionEndTime = 0.f;
		}
	}

	
}
