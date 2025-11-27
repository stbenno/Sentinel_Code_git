// SFW_ShadeAIController.cpp

#include "Core/AI/SFW_ShadeAIController.h"
#include "Core/AI/SFW_ShadeCharacterBase.h"
#include "Core/Game/SFW_GameState.h"
#include "Core/Rooms/RoomVolume.h"
#include "Core/Actors/SFW_DoorBase.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AIPerceptionTypes.h"

#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Engine/TargetPoint.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

ASFW_ShadeAIController::ASFW_ShadeAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- Perception setup ---
	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	if (SightConfig)
	{
		SightConfig->SightRadius = 1500.f;
		SightConfig->LoseSightRadius = 1800.f;
		SightConfig->PeripheralVisionAngleDegrees = 60.f;
		SightConfig->SetMaxAge(2.0f);

		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		Perception->ConfigureSense(*SightConfig);
		Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	SetPerceptionComponent(*Perception);

	if (Perception)
	{
		Perception->OnTargetPerceptionUpdated.AddDynamic(
			this,
			&ASFW_ShadeAIController::OnTargetPerceptionUpdated
		);
	}

	AIState = EShadeAIState::Patrol;
}

void ASFW_ShadeAIController::BeginPlay()
{
	Super::BeginPlay();
}

void ASFW_ShadeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Shade = Cast<ASFW_ShadeCharacterBase>(InPawn);
	AIState = EShadeAIState::Patrol;
	TravelRetryCount = 0;
	NextDoorCheckTime = 0.0;
	GetWorldTimerManager().ClearTimer(TravelRetryHandle);

	// === Early-game: make Shade "ethereal" (invisible + no player collision) ===
	if (ACharacter* Char = Cast<ACharacter>(InPawn))
	{
		if (USkeletalMeshComponent* Mesh = Char->GetMesh())
		{
			Mesh->SetVisibility(false, true); // invisible but still ticking
		}

		if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			// Keep world/static collisions, just ignore Pawns
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}
	}

	// === Pull Base/Rift from GameState ===
	if (UWorld* World = GetWorld())
	{
		if (ASFW_GameState* GS = World->GetGameState<ASFW_GameState>())
		{
			BaseRoom = Cast<ARoomVolume>(GS->BaseRoom);
			RiftRoom = Cast<ARoomVolume>(GS->RiftRoom);

			if (BaseRoom)
			{
				PatrolCenter = BaseRoom->GetActorLocation();
			}

			if (RiftRoom)
			{
				RiftCenter = RiftRoom->GetActorLocation();
			}

			UE_LOG(LogTemp, Warning,
				TEXT("[ShadeAI] OnPossess: BaseRoom=%s Center=%s | RiftRoom=%s Center=%s"),
				*GetNameSafe(BaseRoom),
				*PatrolCenter.ToString(),
				*GetNameSafe(RiftRoom),
				*RiftCenter.ToString());
		}
	}

	// Fallback if no BaseRoom (or GS not set) – use spawn location.
	if (PatrolCenter.IsNearlyZero() && Shade)
	{
		PatrolCenter = Shade->GetActorLocation();
	}

	// If we have a Rift, start by traveling there. Otherwise patrol Base.
	if (!RiftCenter.IsNearlyZero())
	{
		EnterTravelToRift();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShadeAI] OnPossess: No valid RiftCenter, starting Patrol around %s"),
			*PatrolCenter.ToString());
		EnterPatrol();
	}
}

void ASFW_ShadeAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Shade) return;

	// Debug draw even when Shade is invisible so we can see what it's doing
	DebugDrawShade();

	// While traveling to Rift, periodically check for a blocking door in front
	if (AIState == EShadeAIState::TravelToRift)
	{
		if (UWorld* World = GetWorld())
		{
			const double Now = World->GetTimeSeconds();
			if (Now >= NextDoorCheckTime)
			{
				TryResolveDoorBlockage();
				NextDoorCheckTime = Now + DoorCheckInterval;
			}
		}
	}

	if (AIState == EShadeAIState::Chase && TargetActor)
	{
		const FVector ShadeLoc = Shade->GetActorLocation();
		const FVector TargetLoc = TargetActor->GetActorLocation();

		LastKnownPos = TargetLoc; // keep updating last known

		const float DistSq = FVector::DistSquared(ShadeLoc, TargetLoc);
		if (DistSq <= FMath::Square(AttackDistance))
		{
			TryAttackTarget();
		}
	}
}

// ======================================================
// Debug draw
// ======================================================

void ASFW_ShadeAIController::DebugDrawShade() const
{
	if (!bDebugDrawPath) return;
	if (!Shade) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Loc = Shade->GetActorLocation();
	const float Lifetime = 0.15f;
	const float Radius = 25.f;

	FColor Color = FColor::White;
	switch (AIState)
	{
	case EShadeAIState::TravelToRift: Color = FColor::Cyan;   break;
	case EShadeAIState::Patrol:       Color = FColor::Green;  break;
	case EShadeAIState::Chase:        Color = FColor::Red;    break;
	case EShadeAIState::Search:       Color = FColor::Yellow; break;
	default:                          Color = FColor::White;  break;
	}

	// Sphere at Shade's current location
	DrawDebugSphere(
		World,
		Loc,
		Radius,
		12,
		Color,
		false,
		Lifetime,
		0,
		2.0f
	);

	// Rift marker + line so you can see the intent
	if (!RiftCenter.IsNearlyZero())
	{
		DrawDebugSphere(
			World,
			RiftCenter,
			40.f,
			16,
			FColor::Purple,
			false,
			Lifetime,
			0,
			2.0f
		);

		DrawDebugLine(
			World,
			Loc,
			RiftCenter,
			Color,
			false,
			Lifetime,
			0,
			1.5f
		);
	}
}

// ======================================================
// Door handling while traveling
// ======================================================

void ASFW_ShadeAIController::TryResolveDoorBlockage()
{
	if (!Shade) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Start = Shade->GetActorLocation();
	FVector Forward = Shade->GetActorForwardVector();
	if (!Forward.Normalize())
	{
		return;
	}

	const FVector End = Start + Forward * DoorProbeDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShadeDoorProbe), false, Shade);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return;
	}

	ASFW_DoorBase* Door = nullptr;

	if (AActor* HitActor = Hit.GetActor())
	{
		Door = Cast<ASFW_DoorBase>(HitActor);

		if (!Door)
		{
			if (UPrimitiveComponent* HitComp = Hit.GetComponent())
			{
				Door = Cast<ASFW_DoorBase>(HitComp->GetOwner());
			}
		}
	}

	if (!Door)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[ShadeAI] TryResolveDoorBlockage: Found door %s, calling OpenDoor()"),
		*Door->GetName());

	// If it's not locked, open it so the Shade can pass.
	if (!Door->IsLocked())
	{
		Door->OpenDoor();
	}

	// IMPORTANT:
	// Do NOT call EnterTravelToRift() here.
	// Reissuing MoveToLocation would abort the current request and
	// spam Result=Aborted; the existing move will keep running, and
	// if the path still fails, OnMoveCompleted will schedule a retry.
}


// ======================================================
// Perception
// ======================================================

void ASFW_ShadeAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Early phase – completely ignore players so we can debug pathing.
	if (bIgnorePlayers)
	{
		return;
	}

	if (!Actor) return;

	APawn* SensedPawn = Cast<APawn>(Actor);
	if (!SensedPawn || !SensedPawn->IsPlayerControlled())
	{
		return; // ignore non-player things for now
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// Saw / re-saw a player
		LastKnownPos = Actor->GetActorLocation();
		UE_LOG(LogTemp, Warning, TEXT("[ShadeAI] Perception: Saw player %s, entering Chase"), *Actor->GetName());
		EnterChase(Actor);
	}
	else
	{
		// Lost sight of the player
		LastKnownPos = Stimulus.StimulusLocation;

		if (AIState == EShadeAIState::Chase)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ShadeAI] Perception: Lost sight of player, entering Search at %s"),
				*LastKnownPos.ToString());
			EnterSearch(LastKnownPos);
		}
	}
}

// ======================================================
// Patrol helpers
// ======================================================

void ASFW_ShadeAIController::StartPatrol()
{
	if (!Shade) return;

	// If we somehow lost our center, recenter on Shade
	if (PatrolCenter.IsNearlyZero())
	{
		PatrolCenter = Shade->GetActorLocation();
	}

	MoveToNextPatrolPoint();
}

void ASFW_ShadeAIController::MoveToNextPatrolPoint()
{
	if (!Shade || bPatrolLegActive) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return;

	const FVector Center = PatrolCenter.IsNearlyZero()
		? Shade->GetActorLocation()
		: PatrolCenter;

	FNavLocation Dest;
	if (!NavSys->GetRandomReachablePointInRadius(Center, PatrolRadius, Dest))
	{
		// Couldn’t find a point; try again later
		UE_LOG(LogTemp, Verbose,
			TEXT("[ShadeAI] MoveToNextPatrolPoint: Failed to find random point near %s"),
			*Center.ToString());
		return;
	}

	bPatrolLegActive = true;
	CurrentGoalIndex++; // just for debug bookkeeping if needed

	UE_LOG(LogTemp, Verbose,
		TEXT("[ShadeAI] Patrol: Moving to %s (Center=%s Radius=%.1f)"),
		*Dest.Location.ToString(),
		*Center.ToString(),
		PatrolRadius);

	MoveToLocation(Dest.Location);
}

void ASFW_ShadeAIController::StartPatrolWait()
{
	bPatrolLegActive = false;

	if (UWorld* World = GetWorld())
	{
		const float WaitTime = FMath::FRandRange(PatrolWaitMin, PatrolWaitMax);

		UE_LOG(LogTemp, Verbose,
			TEXT("[ShadeAI] Patrol: Waiting %.2f seconds before next leg"), WaitTime);

		World->GetTimerManager().SetTimer(
			PatrolWaitHandle,
			this,
			&ASFW_ShadeAIController::MoveToNextPatrolPoint,
			WaitTime,
			false
		);
	}
}

void ASFW_ShadeAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// --- Travel to Rift handling ---
	if (AIState == EShadeAIState::TravelToRift)
	{
		const int32 Code = static_cast<int32>(Result.Code);
		UE_LOG(LogTemp, Warning,
			TEXT("[ShadeAI] OnMoveCompleted: TravelToRift Result=%d"), Code);

		if (Result.Code == EPathFollowingResult::Success)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ShadeAI] Reached Rift, switching to Patrol at RiftCenter=%s"),
				*RiftCenter.ToString());

			if (!RiftCenter.IsNearlyZero())
			{
				PatrolCenter = RiftCenter;

				if (Shade)
				{
					Shade->InitializeHome(RiftCenter);
				}
			}

			// Successful arrival; clear retries
			TravelRetryCount = 0;
			GetWorldTimerManager().ClearTimer(TravelRetryHandle);

			EnterPatrol();
		}
		else
		{
			FVector FallbackLoc = Shade ? Shade->GetActorLocation() : FVector::ZeroVector;

			UE_LOG(LogTemp, Warning,
				TEXT("[ShadeAI] Failed to reach Rift (Result=%d) at loc=%s"),
				Code,
				*FallbackLoc.ToString());

			if (UWorld* World = GetWorld())
			{
				if (TravelRetryCount < MaxTravelRetries)
				{
					TravelRetryCount++;
					UE_LOG(LogTemp, Warning,
						TEXT("[ShadeAI] Scheduling TravelToRift retry %d in %.2f sec"),
						TravelRetryCount,
						TravelRetryDelay);

					World->GetTimerManager().SetTimer(
						TravelRetryHandle,
						this,
						&ASFW_ShadeAIController::EnterTravelToRift,
						TravelRetryDelay,
						false
					);
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[ShadeAI] MaxTravelRetries reached, giving up and patrolling Base at %s"),
						*FallbackLoc.ToString());

					if (PatrolCenter.IsNearlyZero())
					{
						PatrolCenter = FallbackLoc;
					}

					EnterPatrol();
				}
			}
		}
		return;
	}

	// --- Normal Patrol handling ---
	if (AIState != EShadeAIState::Patrol)
	{
		return;
	}

	if (Result.Code == EPathFollowingResult::Success)
	{
		StartPatrolWait();
	}
	else
	{
		// Failed path: immediately try another point
		bPatrolLegActive = false;
		MoveToNextPatrolPoint();
	}
}

// ======================================================
// State transitions
// ======================================================

void ASFW_ShadeAIController::EnterTravelToRift()
{
	if (!Shade)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ShadeAI] EnterTravelToRift: No Shade pawn, falling back to Patrol"));
		EnterPatrol();
		return;
	}

	if (RiftCenter.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ShadeAI] EnterTravelToRift: No valid RiftCenter, falling back to Patrol"));
		EnterPatrol();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		EnterPatrol();
		return;
	}

	FVector Dest = RiftCenter;

	// Project RiftCenter to the NavMesh
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation Projected;
		// Box extent gives us some room to find nearby nav
		if (NavSys->ProjectPointToNavigation(RiftCenter, Projected, FVector(500.f, 500.f, 500.f)))
		{
			Dest = Projected.Location;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ShadeAI] EnterTravelToRift: ProjectPointToNavigation failed for %s, falling back to Patrol"),
				*RiftCenter.ToString());
			EnterPatrol();
			return;
		}
	}

	AIState = EShadeAIState::TravelToRift;

	UE_LOG(LogTemp, Warning,
		TEXT("[ShadeAI] EnterTravelToRift: Moving to NavDest=%s (OriginalRiftCenter=%s, AcceptanceRadius=%.1f)"),
		*Dest.ToString(),
		*RiftCenter.ToString(),
		TravelAcceptanceRadius);

	GetWorldTimerManager().ClearTimer(SearchTimerHandle);
	GetWorldTimerManager().ClearTimer(PatrolWaitHandle);

	bPatrolLegActive = false;
	CurrentGoalIndex = INDEX_NONE;

	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);

	MoveToLocation(Dest, TravelAcceptanceRadius, true, true, true, true);
}

void ASFW_ShadeAIController::EnterPatrol()
{
	AIState = EShadeAIState::Patrol;

	UE_LOG(LogTemp, Warning,
		TEXT("[ShadeAI] EnterPatrol: Center=%s Radius=%.1f"),
		*PatrolCenter.ToString(),
		PatrolRadius);

	// Clear any travel/chase/search timers
	GetWorldTimerManager().ClearTimer(SearchTimerHandle);
	GetWorldTimerManager().ClearTimer(PatrolWaitHandle);
	GetWorldTimerManager().ClearTimer(TravelRetryHandle);

	bPatrolLegActive = false;
	CurrentGoalIndex = INDEX_NONE;

	StartPatrol();
}


void ASFW_ShadeAIController::EnterChase(AActor* NewTarget)
{
	if (!NewTarget || !Shade) return;

	AIState = EShadeAIState::Chase;
	TargetActor = NewTarget;

	UE_LOG(LogTemp, Warning,
		TEXT("[ShadeAI] EnterChase: Target=%s"), *NewTarget->GetName());

	GetWorldTimerManager().ClearTimer(SearchTimerHandle);
	GetWorldTimerManager().ClearTimer(PatrolWaitHandle);

	StopMovement();
	SetFocus(TargetActor);

	if (Shade)
	{
		Shade->SetShadeState(EShadeState::Chase);
	}

	// Keep close but not clipping through
	MoveToActor(TargetActor, AttackDistance * 0.8f, true, true, true, nullptr, true);
}

void ASFW_ShadeAIController::EnterSearch(const FVector& LastKnown)
{
	if (!GetWorld() || !Shade) return;

	AIState = EShadeAIState::Search;
	LastKnownPos = LastKnown;

	UE_LOG(LogTemp, Warning,
		TEXT("[ShadeAI] EnterSearch: LastKnown=%s"), *LastKnownPos.ToString());

	TargetActor = nullptr;
	ClearFocus(EAIFocusPriority::Gameplay);
	StopMovement();

	if (Shade)
	{
		Shade->SetShadeState(EShadeState::Idle); // "searching/idle" blend for now
	}

	// Move to last known location once
	MoveToLocation(LastKnownPos, /*AcceptanceRadius=*/80.f, true, true, true, true);

	// After SearchDuration, go back to patrol if we haven't re-acquired a target.
	GetWorldTimerManager().ClearTimer(SearchTimerHandle);
	GetWorldTimerManager().SetTimer(
		SearchTimerHandle,
		this,
		&ASFW_ShadeAIController::EnterPatrol,
		SearchDuration,
		false
	);
}

// ======================================================
// Attack stub
// ======================================================

void ASFW_ShadeAIController::TryAttackTarget()
{
	if (!Shade || !TargetActor) return;

	const float DistSq = FVector::DistSquared(
		Shade->GetActorLocation(),
		TargetActor->GetActorLocation()
	);

	if (DistSq > FMath::Square(AttackDistance))
	{
		return;
	}

	// TODO: hook into your kill / grab / scare logic.
	UE_LOG(LogTemp, Warning, TEXT("[ShadeAI] Would attack %s"), *TargetActor->GetName());

	// Example placeholder: after "attack", forget target and search last known position
	EnterSearch(TargetActor->GetActorLocation());
}
