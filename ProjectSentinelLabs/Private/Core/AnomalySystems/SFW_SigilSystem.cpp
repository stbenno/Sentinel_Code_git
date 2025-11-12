// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/AnomalySystems/SFW_SigilSystem.h"
#include "Core/AnomalySystems/SFW_SigilActor.h"
#include "Core/Rooms/RoomVolume.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ASFW_SigilSystem::ASFW_SigilSystem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ASFW_SigilSystem::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GatherAllSigils();
	}
}

void ASFW_SigilSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_SigilSystem, bPuzzleActive);
	DOREPLIFETIME(ASFW_SigilSystem, bPuzzleComplete);
	DOREPLIFETIME(ASFW_SigilSystem, NumRealCleansed);
}

void ASFW_SigilSystem::GatherAllSigils()
{
	AllSigils.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASFW_SigilActor> It(World); It; ++It)
	{
		ASFW_SigilActor* Sigil = *It;
		if (IsValid(Sigil))
		{
			AllSigils.Add(Sigil);
		}
	}
}

void ASFW_SigilSystem::GatherSigilsInRoom(FName RoomId, TArray<ASFW_SigilActor*>& OutSigils)
{
	OutSigils.Reset();

	UWorld* World = GetWorld();
	if (!World || RoomId.IsNone())
	{
		return;
	}

	ARoomVolume* TargetRoom = nullptr;

	// Find the room volume by RoomId
	for (TActorIterator<ARoomVolume> It(World); It; ++It)
	{
		ARoomVolume* Vol = *It;
		if (Vol && Vol->RoomId == RoomId)
		{
			TargetRoom = Vol;
			break;
		}
	}

	if (!TargetRoom)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SigilSystem] No ARoomVolume found for RoomId '%s'"),
			*RoomId.ToString());
		return;
	}

	// Ensure global list is built
	if (AllSigils.Num() == 0)
	{
		GatherAllSigils();
	}

	// Use the room's component bounds to test positions
	const FBox RoomBounds = TargetRoom->GetComponentsBoundingBox(true);

	for (TWeakObjectPtr<ASFW_SigilActor>& WeakSigil : AllSigils)
	{
		ASFW_SigilActor* Sigil = WeakSigil.Get();
		if (!Sigil)
		{
			continue;
		}

		const FVector Loc = Sigil->GetActorLocation();
		if (RoomBounds.IsInside(Loc))
		{
			OutSigils.Add(Sigil);
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[SigilSystem] GatherSigilsInRoom RoomId='%s' Found=%d"),
		*RoomId.ToString(),
		OutSigils.Num());
}



void ASFW_SigilSystem::BuildRandomLayoutFromPool(const TArray<ASFW_SigilActor*>& PoolIn)
{
	if (!HasAuthority())
	{
		return;
	}

	ActiveSigils.Reset();

	TArray<ASFW_SigilActor*> Pool = PoolIn;
	if (Pool.Num() < RequiredVisibleSigils)
	{
		return;
	}

	// Randomly pick RequiredVisibleSigils from Pool
	while (ActiveSigils.Num() < RequiredVisibleSigils && Pool.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
		ASFW_SigilActor* Chosen = Pool[Index];
		ActiveSigils.Add(Chosen);
		Pool.RemoveAtSwap(Index);
	}

	// Mark all active ones as fake first
	for (TWeakObjectPtr<ASFW_SigilActor>& WeakSigil : ActiveSigils)
	{
		if (ASFW_SigilActor* S = WeakSigil.Get())
		{
			S->ActivateAsFake();
		}
	}

	// Now randomly choose RequiredRealSigils among the active ones to be real
	TArray<ASFW_SigilActor*> RealPool;
	for (TWeakObjectPtr<ASFW_SigilActor>& WeakSigil : ActiveSigils)
	{
		if (ASFW_SigilActor* S = WeakSigil.Get())
		{
			RealPool.Add(S);
		}
	}

	for (int32 i = 0; i < RequiredRealSigils && RealPool.Num() > 0; ++i)
	{
		const int32 Index = FMath::RandRange(0, RealPool.Num() - 1);
		ASFW_SigilActor* RealSigil = RealPool[Index];
		if (RealSigil)
		{
			RealSigil->ActivateAsReal();
		}
		RealPool.RemoveAtSwap(Index);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[SigilSystem] Initialized layout: Active=%d Real=%d"),
		ActiveSigils.Num(),
		RequiredRealSigils);
}

void ASFW_SigilSystem::InitializeLayoutForRoom(FName RoomId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (RequiredVisibleSigils <= 0 || RequiredRealSigils <= 0 || RequiredRealSigils > RequiredVisibleSigils)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SigilSystem] Invalid RequiredVisibleSigils / RequiredRealSigils settings."));
		return;
	}

	if (AllSigils.Num() == 0)
	{
		GatherAllSigils();
	}

	TArray<ASFW_SigilActor*> RoomSigils;
	GatherSigilsInRoom(RoomId, RoomSigils);

	if (RoomSigils.Num() < RequiredVisibleSigils)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SigilSystem] Not enough sigils in RoomId '%s'. Have %d, need at least %d."),
			*RoomId.ToString(),
			RoomSigils.Num(),
			RequiredVisibleSigils);
		return;
	}

	// Full reset of any previous layout state
	for (TWeakObjectPtr<ASFW_SigilActor>& WeakSigil : AllSigils)
	{
		if (ASFW_SigilActor* S = WeakSigil.Get())
		{
			S->ResetSigil();
		}
	}
	ActiveSigils.Reset();
	NumRealCleansed = 0;
	bPuzzleComplete = false;

	BuildRandomLayoutFromPool(RoomSigils);

	bPuzzleActive = (ActiveSigils.Num() >= RequiredVisibleSigils);

	UE_LOG(LogTemp, Log,
		TEXT("[SigilSystem] InitializeLayoutForRoom RoomId='%s' Active=%d"),
		*RoomId.ToString(),
		ActiveSigils.Num());

	// DEBUG: auto-expose active sigils at round start so you can see them
	/*for (TWeakObjectPtr<ASFW_SigilActor>& WeakSigil : ActiveSigils)
	
		if (ASFW_SigilActor* S = WeakSigil.Get())
		{
			// 0.f = start fully visible, then fade using its existing logic
			S->ExposeByUV(0.f);
		}  */
}

void ASFW_SigilSystem::ResetPuzzle()
{
	if (!HasAuthority())
	{
		return;
	}

	for (TWeakObjectPtr<ASFW_SigilActor>& WeakSigil : AllSigils)
	{
		if (ASFW_SigilActor* S = WeakSigil.Get())
		{
			S->ResetSigil();
		}
	}

	ActiveSigils.Reset();
	NumRealCleansed = 0;
	bPuzzleActive = false;
	bPuzzleComplete = false;
}

void ASFW_SigilSystem::NotifySigilCleansed(ASFW_SigilActor* Sigil, bool bWasReal)
{
	if (!HasAuthority() || !Sigil || !bPuzzleActive || bPuzzleComplete)
	{
		return;
	}

	if (!Sigil->IsActive())
	{
		// Sigil not part of current layout; ignore.
		return;
	}

	if (bWasReal)
	{
		NumRealCleansed++;

		UE_LOG(LogTemp, Log,
			TEXT("[SigilSystem] Real sigil cleansed. Count = %d / %d"),
			NumRealCleansed,
			RequiredRealSigils);

		if (NumRealCleansed >= RequiredRealSigils)
		{
			bPuzzleComplete = true;
			bPuzzleActive = false;

			UE_LOG(LogTemp, Log, TEXT("[SigilSystem] Puzzle COMPLETE."));
			OnPuzzleComplete.Broadcast();
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[SigilSystem] Fake sigil cleansed. Puzzle FAILED."));

		// Reset puzzle state; anomaly decides whether to re-init layout or not.
		ResetPuzzle();
		OnPuzzleFailed.Broadcast();
	}
}
