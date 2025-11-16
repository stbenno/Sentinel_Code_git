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

	switch (R->RoomType)
	{
	case ERoomType::Safe:
	case ERoomType::Hallway:
		return true;
	default:
		break; // Base or RiftCandidate are allowed in the pool
	}
	return false;
}

void ASFW_AnomalyController::PickAnomalyType()
{
	// Example: random between Binder and Splitter.
	const int32 Roll = FMath::RandRange(0, 1);
	ActiveAnomalyType = (Roll == 0)
		? ESFWAnomalyType::Binder
		: ESFWAnomalyType::Splitter;

	UE_LOG(LogAnomalyController, Log, TEXT("PickAnomalyType: %d"), static_cast<int32>(ActiveAnomalyType));
}



void ASFW_AnomalyController::PickRooms()
{
	BaseRoom = nullptr;
	RiftRoom = nullptr;

	UWorld* W = GetWorld();
	if (!W) return;

	// Pool = all rooms that are NOT Safe and NOT Hallway
	TArray<ARoomVolume*> Pool;
	for (TActorIterator<ARoomVolume> It(W); It; ++It)
	{
		ARoomVolume* R = *It;
		if (R && !IsForbiddenRoom(R))
		{
			Pool.Add(R);
		}
	}

	if (Pool.Num() < 2)
	{
		UE_LOG(LogAnomalyController, Warning,
			TEXT("PickRooms: Need at least 2 candidates (non-safe, non-hallway). Found=%d"), Pool.Num());
		return;
	}

	// Pick Base from the pool
	const int32 BaseIdx = FMath::RandRange(0, Pool.Num() - 1);
	BaseRoom = Pool[BaseIdx];

	// Rift = any other from the pool
	TArray<ARoomVolume*> RiftPool = Pool;
	RiftPool.RemoveAt(BaseIdx);

	const int32 RiftIdx = FMath::RandRange(0, RiftPool.Num() - 1);
	RiftRoom = RiftPool[RiftIdx];

	UE_LOG(LogAnomalyController, Log,
		TEXT("PickRooms: Base=%s Rift=%s"),
		*GetNameSafe(BaseRoom),
		*GetNameSafe(RiftRoom));
}


void ASFW_AnomalyController::StartRound()
{
	if (!HasAuthority()) return;

	// 1) Decide which anomaly archetype this round uses
	PickAnomalyType();

	// 2) Pick rooms according to RoomType rules
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
		// If added later: G->ActiveAnomalyType = ActiveAnomalyType;
	}

	// 4) Initialize sigil layout for the rift room (Binder uses this; harmless no-op otherwise)
	if (RiftRoom)
	{
		if (UWorld* W = GetWorld())
		{
			ASFW_SigilSystem* SigilSystem = nullptr;
			for (TActorIterator<ASFW_SigilSystem> It(W); It; ++It)
			{
				SigilSystem = *It;
				break; // first one found
			}

			if (SigilSystem)
			{
				SigilSystem->InitializeLayoutForRoom(RiftRoom->RoomId);
				UE_LOG(LogAnomalyController, Log,
					TEXT("StartRound: Initialized sigil layout for RiftRoom=%s"),
					*GetNameSafe(RiftRoom));
			}
			else
			{
				UE_LOG(LogAnomalyController, Warning,
					TEXT("StartRound: No ASFW_SigilSystem found in world; sigil puzzle disabled."));
			}
		}
	}

	UE_LOG(LogAnomalyController, Warning,
		TEXT("StartRound: AnomalyController Spawned. Type=%d"),
		static_cast<int32>(ActiveAnomalyType));
}
