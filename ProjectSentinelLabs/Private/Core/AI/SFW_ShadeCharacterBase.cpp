// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/AI/SFW_ShadeCharacterBase.h"

#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Core/Rooms/RoomVolume.h"

ASFW_ShadeCharacterBase::ASFW_ShadeCharacterBase()
{
    PrimaryActorTick.bCanEverTick = false;

    SetReplicates(true);
    SetReplicateMovement(true);

    // Let AI possess when placed/spawned
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Initial speed
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = BaseWalkSpeed;
    }

    // --- Detection sphere ---
    DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
    DetectionSphere->SetupAttachment(GetRootComponent());
    DetectionSphere->InitSphereRadius(DetectionRadius);
    DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DetectionSphere->SetCanEverAffectNavigation(false);
}

void ASFW_ShadeCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    // If no one explicitly initialized home, assume spawn location.
    if (HomeLocation.IsZero())
    {
        HomeLocation = GetActorLocation();
    }

    ApplyMovementSpeed();

    if (DetectionSphere && HasAuthority())
    {
        DetectionSphere->OnComponentBeginOverlap.AddDynamic(
            this, &ASFW_ShadeCharacterBase::OnDetectionBegin);
        DetectionSphere->OnComponentEndOverlap.AddDynamic(
            this, &ASFW_ShadeCharacterBase::OnDetectionEnd);
    }

    // Server: figure out which room we start in, so DecisionSystem can use it.
    if (HasAuthority())
    {
        RefreshCurrentRoomFromWorld();
    }
}

void ASFW_ShadeCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASFW_ShadeCharacterBase, ShadeState);
    DOREPLIFETIME(ASFW_ShadeCharacterBase, CurrentRoomId);
}

void ASFW_ShadeCharacterBase::SetAggressionFactor(float InFactor)
{
    AggressionFactor = FMath::Clamp(InFactor, 0.f, 1.f);
    ApplyMovementSpeed();
}

void ASFW_ShadeCharacterBase::SetShadeState(EShadeState NewState)
{
    if (ShadeState == NewState)
    {
        return;
    }

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
        const float Target = BaseWalkSpeed +
            (bChasing ? (AggressionFactor * MaxAggroBonusSpeed) : 0.f);
        Move->MaxWalkSpeed = Target;
    }
}

void ASFW_ShadeCharacterBase::InitializeHome(const FVector& InHomeLocation)
{
    HomeLocation = InHomeLocation;
}

// ----- Target handling -----

void ASFW_ShadeCharacterBase::SetTarget(APawn* NewTarget)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentTarget = NewTarget;

    if (CurrentTarget)
    {
        SetShadeState(EShadeState::Chase);
    }
    else
    {
        // Fall back to Patrol when target is cleared
        if (ShadeState == EShadeState::Chase)
        {
            SetShadeState(EShadeState::Patrol);
        }
    }
}

void ASFW_ShadeCharacterBase::ClearTarget()
{
    SetTarget(nullptr);
}

// ----- Detection callbacks -----

void ASFW_ShadeCharacterBase::OnDetectionBegin(
    UPrimitiveComponent* /*OverlappedComponent*/,
    AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/,
    int32                /*OtherBodyIndex*/,
    bool                 /*bFromSweep*/,
    const FHitResult&    /*SweepResult*/)
{
    if (!HasAuthority())
    {
        return;
    }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->IsPlayerControlled())
    {
        return;
    }

    // Acquire target and start chase.
    SetTarget(Pawn);
}

void ASFW_ShadeCharacterBase::OnDetectionEnd(
    UPrimitiveComponent* /*OverlappedComponent*/,
    AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/,
    int32                /*OtherBodyIndex*/)
{
    if (!HasAuthority())
    {
        return;
    }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        return;
    }

    if (Pawn == CurrentTarget)
    {
        // Simple rule: if they’re outside “lose distance”, drop target.
        const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation());
        if (DistSq > FMath::Square(LoseTargetDistance))
        {
            ClearTarget();
        }
    }
}

// ----- Room tracking -----

void ASFW_ShadeCharacterBase::OnRep_CurrentRoomId()
{
    // Client-side hook if you ever want VFX/UI based on Shade room.
    // Currently intentionally empty.
}

void ASFW_ShadeCharacterBase::SetCurrentRoom(ARoomVolume* NewRoom)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentRoomVolume = NewRoom;
    CurrentRoomId = (NewRoom ? NewRoom->RoomId : NAME_None);

    // Optional debug:
    // UE_LOG(LogTemp, Verbose, TEXT("[Shade] %s now in room '%s'"),
    //     *GetName(), *CurrentRoomId.ToString());
}

void ASFW_ShadeCharacterBase::RefreshCurrentRoomFromWorld()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    ARoomVolume* FoundRoom = nullptr;
    const FVector Loc = GetActorLocation();

    for (TActorIterator<ARoomVolume> It(World); It; ++It)
    {
        ARoomVolume* Vol = *It;
        if (!Vol || Vol->RoomId.IsNone())
        {
            continue;
        }

        FVector Origin, Extent;
        Vol->GetActorBounds(true, Origin, Extent);
        const FBox Box(Origin - Extent, Origin + Extent);

        if (Box.IsInside(Loc))
        {
            FoundRoom = Vol;
            break;
        }
    }

    SetCurrentRoom(FoundRoom);
}

void ASFW_ShadeCharacterBase::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);

    if (!HasAuthority() || !OtherActor)
    {
        return;
    }

    if (ARoomVolume* Room = Cast<ARoomVolume>(OtherActor))
    {
        SetCurrentRoom(Room);
    }
}

void ASFW_ShadeCharacterBase::NotifyActorEndOverlap(AActor* OtherActor)
{
    Super::NotifyActorEndOverlap(OtherActor);

    if (!HasAuthority() || !OtherActor)
    {
        return;
    }

    if (ARoomVolume* Room = Cast<ARoomVolume>(OtherActor))
    {
        // Only clear if we’re leaving the room we think we’re in.
        if (Room == CurrentRoomVolume)
        {
            SetCurrentRoom(nullptr);
        }
    }
}
