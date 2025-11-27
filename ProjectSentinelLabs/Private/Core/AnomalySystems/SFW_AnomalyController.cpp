// SFW_AnomalyController.cpp

#include "Core/AnomalySystems/SFW_AnomalyController.h"

#include "Core/Game/SFW_GameState.h"
#include "Core/Rooms/RoomVolume.h"
#include "Core/AnomalySystems/SFW_SigilSystem.h"
#include "Core/AnomalySystems/SFW_AnomalyDecisionSystem.h"
#include "Core/AI/SFW_ShadeCharacterBase.h"
#include "Core/Actors/SFW_DoorBase.h"
#include "Core/Lights/SFW_PowerLibrary.h"
#include "Components/CapsuleComponent.h"


#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY(LogAnomalyController);

ASFW_AnomalyController::ASFW_AnomalyController()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
}

void ASFW_AnomalyController::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    // Hook into the decision system once on the server
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ASFW_AnomalyDecisionSystem> It(World); It; ++It)
        {
            ASFW_AnomalyDecisionSystem* Sys = *It;
            if (Sys)
            {
                Sys->OnDecision.AddUObject(this, &ASFW_AnomalyController::HandleShadeDecision);
                UE_LOG(LogAnomalyController, Log, TEXT("BeginPlay: Bound to AnomalyDecisionSystem %s"), *GetNameSafe(Sys));
                break;
            }
        }
    }
}

ASFW_GameState* ASFW_AnomalyController::GS() const
{
    return GetWorld() ? GetWorld()->GetGameState<ASFW_GameState>() : nullptr;
}

bool ASFW_AnomalyController::HasActiveShade() const
{
    return IsValid(ActiveShade);
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

    // 1) Decide anomaly archetype.
    PickAnomalyType();

    // 2) Pick rooms.
    PickRooms();

    // 3) Mark round state on GameState.
    if (ASFW_GameState* G = GS())
    {
        const float Now = GetWorld()->GetTimeSeconds();

        G->bRoundActive = true;
        G->RoundStartTime = Now;
        G->RoundSeed = FMath::Rand();
        G->BaseRoom = BaseRoom;
        G->RiftRoom = RiftRoom;
        // G->ActiveAnomalyType = ActiveAnomalyType; // if you add later
    }

    // 4) Initialize sigil layout.
    if (RiftRoom)
    {
        if (UWorld* W = GetWorld())
        {
            ASFW_SigilSystem* SigilSystem = nullptr;
            for (TActorIterator<ASFW_SigilSystem> It(W); It; ++It)
            {
                SigilSystem = *It;
                break;
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

    // 5) We *can* pre-spawn, but Shade decisions now also call SpawnInitialShade().
    //    This call is safe because SpawnInitialShade() early-outs if a Shade already exists.
    SpawnInitialShade();

    UE_LOG(LogAnomalyController, Warning,
        TEXT("StartRound: AnomalyController Spawned. Type=%d"),
        static_cast<int32>(ActiveAnomalyType));
}

void ASFW_AnomalyController::SpawnInitialShade()
{
    if (!HasAuthority())
    {
        return;
    }

    if (HasActiveShade())
    {
        UE_LOG(LogAnomalyController, Verbose,
            TEXT("SpawnInitialShade: Shade already active (%s), skipping."),
            *GetNameSafe(ActiveShade));
        return;
    }

    if (!ShadeClass)
    {
        UE_LOG(LogAnomalyController, Warning, TEXT("SpawnInitialShade: ShadeClass is null."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Use the BaseRoom center as the origin for the spawn
    FVector Origin = GetActorLocation();

    if (BaseRoom)
    {
        Origin = BaseRoom->GetActorLocation();
    }
    else
    {
        UE_LOG(LogAnomalyController, Warning, TEXT("SpawnInitialShade: No BaseRoom set, using controller location."));
    }

    FVector SpawnLocation = Origin;

    // Project onto navmesh / pick a random reachable point so we don't land on tables, beds, etc.
    if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        FNavLocation NavLoc;
        // Radius in cm around the room center – tweak if needed
        const float Radius = 400.f;

        if (NavSys->GetRandomPointInNavigableRadius(Origin, Radius, NavLoc))
        {
            SpawnLocation = NavLoc.Location;
        }
        else
        {
            UE_LOG(LogAnomalyController, Warning,
                TEXT("SpawnInitialShade: GetRandomPointInNavigableRadius failed, using Origin."));
        }
    }

    // Lift by capsule half-height so we don't spawn half inside the floor
    float ExtraZ = 0.f;
    if (ASFW_ShadeCharacterBase* ShadeCDO = Cast<ASFW_ShadeCharacterBase>(ShadeClass->GetDefaultObject()))
    {
        if (UCapsuleComponent* Capsule = ShadeCDO->GetCapsuleComponent())
        {
            ExtraZ = Capsule->GetUnscaledCapsuleHalfHeight() + 5.f;
        }
    }
    SpawnLocation.Z += ExtraZ;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    ActiveShade = World->SpawnActor<ASFW_ShadeCharacterBase>(ShadeClass, SpawnLocation, FRotator::ZeroRotator, Params);

    if (ActiveShade)
    {
        // Anchor its “home” at the BaseRoom center so AI can patrol around that.
        ActiveShade->InitializeHome(Origin);
        ShadePhase = EShadePhase::RoamingToRift;

        UE_LOG(LogAnomalyController, Warning,
            TEXT("SpawnInitialShade: Spawned Shade %s at %s (Home=%s)"),
            *GetNameSafe(ActiveShade),
            *SpawnLocation.ToString(),
            *Origin.ToString());

        // Open all doors in BaseRoom so the Shade isn't trapped.
        if (BaseRoom)
        {
            const FName BaseId = BaseRoom->RoomId;

            for (TActorIterator<ASFW_DoorBase> It(World); It; ++It)
            {
                ASFW_DoorBase* Door = *It;
                if (!Door) continue;

                if (Door->GetRoomID() == BaseId && !Door->IsLocked())
                {
                    Door->OpenDoor();
                }
            }
        }
    }
    else
    {
        UE_LOG(LogAnomalyController, Warning,
            TEXT("SpawnInitialShade: Failed to spawn shade at %s (collision)"),
            *SpawnLocation.ToString());
    }
}


void ASFW_AnomalyController::HandleShadeDecision(const FSFWDecisionPayload& P)
{
    if (!HasAuthority())
    {
        return;
    }

    switch (P.Type)
    {
    case ESFWDecision::SpawnShade:
    {
        UE_LOG(LogAnomalyController, Log,
            TEXT("HandleShadeDecision: SpawnShade requested (PayloadRoom=%s)."),
            *P.RoomId.ToString());

        if (!HasActiveShade())
        {
            SpawnInitialShade();
        }
        else
        {
            UE_LOG(LogAnomalyController, Verbose,
                TEXT("HandleShadeDecision: SpawnShade ignored; Shade already active."));
        }
        break;
    }

    case ESFWDecision::ShadeAlert:
    {
        UE_LOG(LogAnomalyController, Log,
            TEXT("HandleShadeDecision: ShadeAlert (spy) requested. PayloadRoom=%s"),
            *P.RoomId.ToString());

        if (!HasActiveShade())
        {
            UE_LOG(LogAnomalyController, Verbose,
                TEXT("HandleShadeDecision: ShadeAlert ignored; no active Shade."));
            return;
        }

        ASFW_ShadeCharacterBase* Shade = ActiveShade;
        if (!Shade)
        {
            return;
        }

        APawn* TargetPawn = Shade->GetTarget();
        if (!TargetPawn || !TargetPawn->IsPlayerControlled())
        {
            UE_LOG(LogAnomalyController, Verbose,
                TEXT("HandleShadeDecision: ShadeAlert – Shade has no player target."));
            return;
        }

        UWorld* World = GetWorld();
        if (!World) return;

        // Find room that contains the target
        ARoomVolume* TargetRoom = nullptr;
        for (TActorIterator<ARoomVolume> It(World); It; ++It)
        {
            ARoomVolume* Vol = *It;
            if (!Vol || Vol->RoomId.IsNone()) continue;

            FVector Origin, Extent;
            Vol->GetActorBounds(true, Origin, Extent);
            const FBox Box(Origin - Extent, Origin + Extent);

            if (Box.IsInside(TargetPawn->GetActorLocation()))
            {
                TargetRoom = Vol;
                break;
            }
        }

        if (!TargetRoom)
        {
            UE_LOG(LogAnomalyController, Verbose,
                TEXT("HandleShadeDecision: ShadeAlert – no room found for target."));
            return;
        }

        // Only scare if they’re alone in that room
        TArray<AActor*> Over;
        TargetRoom->GetOverlappingActors(Over, APawn::StaticClass());

        int32 PlayerCount = 0;
        for (AActor* A : Over)
        {
            APawn* Pn = Cast<APawn>(A);
            if (Pn && Pn->IsPlayerControlled())
            {
                ++PlayerCount;
            }
        }

        if (PlayerCount != 1)
        {
            UE_LOG(LogAnomalyController, Verbose,
                TEXT("HandleShadeDecision: ShadeAlert – player not alone (Count=%d), no scare."), PlayerCount);
            return;
        }

        // Fire a small, quick scare in that room (reuse power library for now)
        const float Duration = (P.Duration > 0.f) ? P.Duration : 1.2f;
        USFW_PowerLibrary::FlickerRoom(this, TargetRoom->RoomId, Duration);

        UE_LOG(LogAnomalyController, Log,
            TEXT("HandleShadeDecision: ShadeAlert – flicker scare in RoomId=%s"),
            *TargetRoom->RoomId.ToString());
        break;
    }

    case ESFWDecision::ShadeHunt:
        UE_LOG(LogAnomalyController, Log,
            TEXT("HandleShadeDecision: ShadeHunt requested (stub – not implemented yet)."));
        // TODO: only allow if ShadePhase == EShadePhase::AtRift and we want to escalate to Hunting.
        break;

    case ESFWDecision::ShadeRoam:
        // Roaming is handled purely by Shade AI; no explicit decision needed.
        UE_LOG(LogAnomalyController, Verbose,
            TEXT("HandleShadeDecision: ShadeRoam (no-op; roam is autonomous)."));
        break;

    default:
        // Ignore non-shade decisions; other systems (doors, lights, etc.) handle those.
        break;
    }
}
