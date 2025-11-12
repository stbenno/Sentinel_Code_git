// SFW_AnomalyController.cpp

#include "Core/AnomalySystems/SFW_AnomalyController.h"

#include "Core/Game/SFW_GameState.h"
#include "Core/Rooms/RoomVolume.h"
#include "EngineUtils.h"
#include "Core/AnomalySystems/SFW_SigilSystem.h" 

DEFINE_LOG_CATEGORY(LogAnomalyController);

ASFW_AnomalyController::ASFW_AnomalyController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ASFW_AnomalyController::BeginPlay()
{
	Super::BeginPlay();
}

ASFW_GameState* ASFW_AnomalyController::GS() const
{
	return GetWorld() ? GetWorld()->GetGameState<ASFW_GameState>() : nullptr;
}

bool ASFW_AnomalyController::IsForbiddenRoom(ARoomVolume* R) const
{
	if (!R) return true;

	// Safe rooms and hallways should never be Base/Rift
	if (R->bIsSafeRoom)
	{
		return true;
	}

	static const FName HallName(TEXT("Hallway"));
	if (R->RoomId == HallName)
	{
		return true;
	}

	return false;
}

void ASFW_AnomalyController::PickAnomalyType()
{
	// Simple example: random between Binder and Splitter.
	// You can replace this with weighted logic or external config.
	const int32 Roll = FMath::RandRange(0, 1);
	ActiveAnomalyType = (Roll == 0)
		? ESFWAnomalyType::Binder
		: ESFWAnomalyType::Splitter;

	UE_LOG(
		LogAnomalyController,
		Log,
		TEXT("PickAnomalyType: %d"),
		static_cast<int32>(ActiveAnomalyType)
	);
}

void ASFW_AnomalyController::PickRooms()
{
	BaseRoom = nullptr;
	RiftRoom = nullptr;

	UWorld* W = GetWorld();
	if (!W) return;

	// Collect candidate rooms: not safe, not hallway
	TArray<ARoomVolume*> Candidates;
	for (TActorIterator<ARoomVolume> It(W); It; ++It)
	{
		ARoomVolume* R = *It;
		if (!R) continue;

		if (IsForbiddenRoom(R))
		{
			continue;
		}

		Candidates.Add(R);
	}

	if (Candidates.Num() == 0)
	{
		UE_LOG(
			LogAnomalyController,
			Warning,
			TEXT("PickRooms: No valid room candidates (non-safe, non-hallway).")
		);
		return;
	}

	// Pick Base randomly
	const int32 BaseIdx = FMath::RandRange(0, Candidates.Num() - 1);
	BaseRoom = Candidates[BaseIdx];

	if (Candidates.Num() < 2)
	{
		// Only one candidate; cannot pick a distinct Rift
		RiftRoom = nullptr;
		UE_LOG(
			LogAnomalyController,
			Warning,
			TEXT("PickRooms: Only one valid room candidate. Base=%s Rift=NULL"),
			*GetNameSafe(BaseRoom)
		);
		return;
	}

	// Pick Rift randomly, distinct from Base
	int32 RiftIdx = FMath::RandRange(0, Candidates.Num() - 2);
	if (RiftIdx >= BaseIdx)
	{
		RiftIdx++; // skip over BaseIdx
	}
	RiftRoom = Candidates[RiftIdx];

	UE_LOG(
		LogAnomalyController,
		Log,
		TEXT("PickRooms: Base=%s Rift=%s"),
		*GetNameSafe(BaseRoom),
		*GetNameSafe(RiftRoom)
	);
}

void ASFW_AnomalyController::StartRound()
{
	if (!HasAuthority()) return;

	// 1) Decide which anomaly archetype this round uses
	PickAnomalyType();

	// 2) Pick rooms
	PickRooms();

	// 3) Mark round state on GameState
	if (ASFW_GameState* G = GS())
	{
		const float Now = GetWorld()->GetTimeSeconds();

		G->bRoundActive = true;
		G->RoundStartTime = Now;
		G->RoundSeed = FMath::Rand();
		G->BaseRoom = BaseRoom;
		G->RiftRoom = RiftRoom;

		// G->ActiveAnomalyType = ActiveAnomalyType; // if you add this later
	}

	// 4) Initialize sigil layout for the rift room (for now: always, if RiftRoom exists)
	if (RiftRoom)
	{
		UWorld* W = GetWorld();
		if (W)
		{
			ASFW_SigilSystem* SigilSystem = nullptr;

			for (TActorIterator<ASFW_SigilSystem> It(W); It; ++It)
			{
				SigilSystem = *It;
				break; // use first one found
			}

			if (SigilSystem)
			{
				SigilSystem->InitializeLayoutForRoom(RiftRoom->RoomId);

				UE_LOG(
					LogAnomalyController,
					Log,
					TEXT("StartRound: Initialized sigil layout for RiftRoom=%s"),
					*GetNameSafe(RiftRoom)
				);
			}
			else
			{
				UE_LOG(
					LogAnomalyController,
					Warning,
					TEXT("StartRound: No ASFW_SigilSystem found in world; sigil puzzle disabled.")
				);
			}
		}
	}

	UE_LOG(
		LogAnomalyController,
		Warning,
		TEXT("StartRound: AnomalyController Spawned. Type=%d"),
		static_cast<int32>(ActiveAnomalyType)
	);
}

