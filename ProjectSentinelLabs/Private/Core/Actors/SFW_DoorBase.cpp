// SFW_DoorBase.cpp

#include "Core/Actors/SFW_DoorBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

#include "Core/AnomalySystems/SFW_AnomalyDecisionSystem.h"
#include "Core/Game/SFW_GameState.h"
#include "Core/AI/Scares/SFW_DoorScareFX.h"
#include "Core/AI/SFW_ShadeCharacterBase.h"

ASFW_DoorBase::ASFW_DoorBase()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);
	bReplicates = true;

	Frame = CreateDefaultSubobject<USceneComponent>(TEXT("Frame"));
	SetRootComponent(Frame);

	Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	Door->SetupAttachment(Frame);
	Door->SetMobility(EComponentMobility::Movable);

	// Important: do NOT let the door mesh affect navmesh – pathfinding should
	// always see a clear corridor through the doorway.
	Door->SetCanEverAffectNavigation(false);

	// Proximity (for scare + AI auto-open)
	ProximityTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("ProximityTrigger"));
	ProximityTrigger->SetupAttachment(Frame);
	ProximityTrigger->InitSphereRadius(90.f);
	ProximityTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProximityTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProximityTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProximityTrigger->SetCanEverAffectNavigation(false);

	// Dedicated interaction hit box (line-trace friendly, static size per door)
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(Door);                     // swings with the door
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	InteractionBox->bHiddenInGame = true;
	InteractionBox->SetCanEverAffectNavigation(false);

	ScareFXAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ScareFXAnchor"));
	ScareFXAnchor->SetupAttachment(Frame);
}

void ASFW_DoorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Configure interaction box shape/offset
	if (InteractionBox)
	{
		InteractionBox->SetBoxExtent(InteractionBoxExtent * 0.5f);
		InteractionBox->SetRelativeLocation(InteractionBoxOffset);
	}

	const float Yaw = (InitialState == EDoorState::Open) ? OpenYaw : ClosedYaw;
	SnapTo(Yaw);
	State = InitialState;
}

void ASFW_DoorBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		EDoorState StartState = InitialState;
		if (bRandomizeInitialState)
		{
			StartState = (FMath::FRand() < InitialOpenChance) ? EDoorState::Open : EDoorState::Closed;
		}
		State = StartState;
		ApplyState();

		for (TActorIterator<ASFW_AnomalyDecisionSystem> It(GetWorld()); It; ++It)
		{
			It->OnDecision.AddUObject(this, &ASFW_DoorBase::HandleDecision);
			break;
		}
	}

	if (ProximityTrigger)
	{
		ProximityTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASFW_DoorBase::OnProximityBegin);
	}

	ApplyState();
}

void ASFW_DoorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (TActorIterator<ASFW_AnomalyDecisionSystem> It(GetWorld()); It; ++It)
		{
			It->OnDecision.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ASFW_DoorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASFW_DoorBase, State);
	DOREPLIFETIME(ASFW_DoorBase, LockEndTime);
}

void ASFW_DoorBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State == EDoorState::Opening || State == EDoorState::Closing)
	{
		const float Dur = FMath::Max(AnimDuration, KINDA_SMALL_NUMBER);
		AnimT = FMath::Min(AnimT + (DeltaSeconds / Dur), 1.f);
		const float NewYaw = FMath::Lerp(StartYaw, TargetYaw, AnimT);
		SnapTo(NewYaw);

		if (AnimT >= 1.f)
		{
			FinishMotion();
		}
	}
}

void ASFW_DoorBase::OnRep_State()
{
	ApplyState();
}

bool ASFW_DoorBase::IsLocked() const
{
	const UWorld* W = GetWorld();
	return W && (W->GetTimeSeconds() < LockEndTime);
}

float ASFW_DoorBase::GetYaw() const
{
	return Door ? Door->GetRelativeRotation().Yaw : 0.f;
}

void ASFW_DoorBase::SnapTo(float YawDeg)
{
	if (!Door) return;
	FRotator R = Door->GetRelativeRotation();
	R.Yaw = YawDeg;
	Door->SetRelativeRotation(R);
}

void ASFW_DoorBase::SetDoorCollisionForAnimation(bool bAnimating)
{
	if (!Door) return;

	if (bAnimating)
	{
		// While swinging, don't physically block characters.
		Door->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Door->SetCollisionResponseToAllChannels(ECR_Block);
		Door->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	else
	{
		// After the door is fully open/closed, restore solid collision.
		Door->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Door->SetCollisionResponseToAllChannels(ECR_Block);
	}
}

void ASFW_DoorBase::ApplyState()
{
	const float Current = GetYaw();

	if (State == EDoorState::Opening || State == EDoorState::Closing)
	{
		const float Goal = (State == EDoorState::Opening) ? OpenYaw : ClosedYaw;
		StartYaw = Current;
		TargetYaw = Goal;
		AnimT = 0.f;

		// While animating, let Shade slide through if it happens to be in the doorway.
		SetDoorCollisionForAnimation(true);

		SetActorTickEnabled(true);
	}
	else
	{
		SetActorTickEnabled(false);
		SnapTo(State == EDoorState::Open ? OpenYaw : ClosedYaw);

		// Static state: restore full collision.
		SetDoorCollisionForAnimation(false);
	}
}

void ASFW_DoorBase::FinishMotion()
{
	const bool bToOpen = FMath::IsNearlyEqual(TargetYaw, OpenYaw, 0.5f);

	if (HasAuthority())
	{
		State = bToOpen ? EDoorState::Open : EDoorState::Closed;
		ApplyState();
	}
	else
	{
		SnapTo(bToOpen ? OpenYaw : ClosedYaw);
		SetActorTickEnabled(false);
		SetDoorCollisionForAnimation(false);
	}
}

void ASFW_DoorBase::OpenDoor()
{
	UE_LOG(LogTemp, Warning, TEXT("[Door] %s OpenDoor State=%d Locked=%d Role=%d"),
		*GetName(), (int32)State, IsLocked() ? 1 : 0, (int32)GetLocalRole());

	if (IsLocked()) return;
	if (State == EDoorState::Open || State == EDoorState::Opening) return;
	if (!HasAuthority()) return;

	State = EDoorState::Opening;
	ApplyState();
}

void ASFW_DoorBase::CloseDoor()
{
	UE_LOG(LogTemp, Warning, TEXT("[Door] %s CloseDoor State=%d Locked=%d Role=%d"),
		*GetName(), (int32)State, IsLocked() ? 1 : 0, (int32)GetLocalRole());

	if (State == EDoorState::Closed || State == EDoorState::Closing) return;
	if (!HasAuthority()) return;

	State = EDoorState::Closing;
	ApplyState();
}

void ASFW_DoorBase::LockDoor(float Duration)
{
	if (!HasAuthority()) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LockEndTime = Now + FMath::Max(Duration, 0.f);

	// When locking, ensure we close the door.
	State = EDoorState::Closing;
	ApplyState();
}

void ASFW_DoorBase::Unlock()
{
	if (!HasAuthority()) return;
	LockEndTime = 0.f;
}

// === Player interaction support ===
void ASFW_DoorBase::Interact_Implementation(AController* /*Controller*/)
{
	if (!HasAuthority())
	{
		Server_PlayerToggle(nullptr);
		return;
	}

	if (!CanPlayerToggle())
		return;

	if (State == EDoorState::Open || State == EDoorState::Opening)
	{
		CloseDoor();
	}
	else
	{
		OpenDoor();
	}

	if (UWorld* W = GetWorld())
	{
		NextPlayerToggleTime = W->GetTimeSeconds() + PlayerToggleCooldown;
	}
}

void ASFW_DoorBase::Server_PlayerToggle_Implementation(AController* /*Controller*/)
{
	if (!CanPlayerToggle())
		return;

	if (State == EDoorState::Open || State == EDoorState::Opening)
	{
		CloseDoor();
	}
	else
	{
		OpenDoor();
	}

	if (UWorld* W = GetWorld())
	{
		NextPlayerToggleTime = W->GetTimeSeconds() + PlayerToggleCooldown;
	}
}

bool ASFW_DoorBase::CanPlayerToggle() const
{
	const UWorld* W = GetWorld();
	const double Now = W ? W->GetTimeSeconds() : 0.0;

	if (IsLocked()) return false;
	if (State == EDoorState::Opening || State == EDoorState::Closing) return false;
	if (Now < NextPlayerToggleTime) return false;

	return true;
}

// --- Scare logic ---
void ASFW_DoorBase::OnProximityBegin(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/
)
{
	if (!HasAuthority()) return;

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;

	// Player: scare logic as before
	if (Pawn->IsPlayerControlled())
	{
		TryDoorScare(Pawn, false);
	}

	// Shade / AI: auto-open doors in front of it
	if (!Pawn->IsPlayerControlled())
	{
		if (Pawn->IsA(ASFW_ShadeCharacterBase::StaticClass()))
		{
			if (!IsLocked() && State == EDoorState::Closed)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[Door] Auto-opening for Shade: %s"), *GetName());
				OpenDoor();
			}
		}
	}
}

void ASFW_DoorBase::TryDoorScare(APawn* Pawn, bool bForce)
{
	if (!HasAuthority() || !Pawn) return;

	if (!bForce)
	{
		if (State != EDoorState::Open) return;
		if (IsLocked()) return;

		const float Now = GetWorld()->GetTimeSeconds();
		if ((Now - LastScareTime) < ScareCooldown) return;

		if (FMath::FRand() > ScareChance) return;
		LastScareTime = Now;
	}

	StartSlamSequence(Pawn);
}

void ASFW_DoorBase::StartSlamSequence(APawn* /*Pawn*/)
{
	const FTransform Where = ComputeScareFXTransform();
	Multicast_PlaySlamFX(Where);
	GetWorldTimerManager().SetTimer(
		Timer_SlamImpact,
		this,
		&ASFW_DoorBase::OnSlamImpact,
		SlamImpactDelay,
		false
	);
}

void ASFW_DoorBase::OnSlamImpact()
{
	if (!HasAuthority()) return;

	const FVector Loc = Door ? Door->GetComponentLocation() : GetActorLocation();
	Multicast_PlaySlamSFX(Loc);

	State = EDoorState::Closing;
	ApplyState();
	LockDoor(ScareLockDuration);
}

FTransform ASFW_DoorBase::ComputeScareFXTransform() const
{
	const FTransform Base = ScareFXAnchor ? ScareFXAnchor->GetComponentTransform() : GetActorTransform();
	const FVector WorldLoc = Base.TransformPosition(ScareFXLocationOffset);
	const FQuat   WorldRot = Base.GetRotation() * ScareFXRotationOffset.Quaternion();
	return FTransform(WorldRot, WorldLoc, FVector(1.f));
}

void ASFW_DoorBase::Multicast_PlaySlamFX_Implementation(const FTransform& Where)
{
	if (!ScareFXClass) return;

	ASFW_DoorScareFX* FX = GetWorld()->SpawnActorDeferred<ASFW_DoorScareFX>(
		ScareFXClass,
		Where,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	if (FX)
	{
		FX->Lifetime = ScareFXLifetime;
		FX->FinishSpawning(Where);

		if (bAttachFXToAnchor && ScareFXAnchor)
		{
			FX->AttachToComponent(ScareFXAnchor, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void ASFW_DoorBase::Multicast_PlaySlamSFX_Implementation(const FVector& Loc)
{
	if (SlamSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SlamSFX, Loc);
	}
}

void ASFW_DoorBase::HandleDecision_Implementation(const FSFWDecisionPayload& P)
{
	// Server only, and only if this door is in the targeted room
	if (!HasAuthority() || P.RoomId != RoomID)
	{
		return;
	}

	switch (P.Type)
	{
	case ESFWDecision::OpenDoor:
		OpenDoor();
		break;

	case ESFWDecision::CloseDoor:
	case ESFWDecision::CloseAllDoorsInRoom:
		CloseDoor();
		break;

	case ESFWDecision::LockDoor:
	case ESFWDecision::LockAllDoorsInRoom:
	case ESFWDecision::JamDoor:
		LockDoor(P.Duration);
		break;

	case ESFWDecision::DoorSlamScare:
		StartSlamSequence(nullptr);
		break;

	default:
		break;
	}
}
