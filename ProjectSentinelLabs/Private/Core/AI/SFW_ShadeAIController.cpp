// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/AI/SFW_ShadeAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AIPerceptionTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TargetPoint.h"
#include "Core/AI/SFW_ShadeCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "AIController.h"



ASFW_ShadeAIController::ASFW_ShadeAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// Sight defaults (tune later / scale with aggression)
	SightConfig->SightRadius = 2000.f;
	SightConfig->LoseSightRadius = 2300.f;
	SightConfig->PeripheralVisionAngleDegrees = 80.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void ASFW_ShadeAIController::BeginPlay()
{
	Super::BeginPlay();

	if (Perception)
	{
		Perception->OnTargetPerceptionUpdated.AddDynamic(this, &ASFW_ShadeAIController::OnTargetPerceptionUpdated);
	}
}

void ASFW_ShadeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Shade = Cast<ASFW_ShadeCharacterBase>(InPawn);

	BuildPatrolPointList();
	EnterPatrol();
}

void ASFW_ShadeAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// No per-tick MoveTo calls — movement handled on state entry or OnMoveCompleted
}

void ASFW_ShadeAIController::EnterPatrol()
{
	AIState = EShadeAIState::Patrol;
	TargetActor = nullptr;

	// Stop any timers
	GetWorld()->GetTimerManager().ClearTimer(SearchTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitHandle);

	// Reset patrol leg state
	bPatrolLegActive = false;
	CurrentGoalIndex = INDEX_NONE;

	StopMovement();
	if (Shade)
	{
		Shade->SetShadeState(EShadeState::Patrol);
		Shade->SetAggressionFactor(0.0f);
	}

	// Wait briefly before moving to the next point
	StartPatrolWait();
}

void ASFW_ShadeAIController::EnterChase(AActor* NewTarget)
{
	if (!IsValid(NewTarget)) return;

	AIState = EShadeAIState::Chase;
	TargetActor = NewTarget;

	// Stop any timers and reset patrol leg state
	GetWorld()->GetTimerManager().ClearTimer(SearchTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitHandle);
	bPatrolLegActive = false;
	CurrentGoalIndex = INDEX_NONE;

	if (Shade)
	{
		Shade->SetShadeState(EShadeState::Chase);
		Shade->SetAggressionFactor(1.0f);
	}

	MoveToActor(TargetActor, 100.f);
}

void ASFW_ShadeAIController::EnterSearch(const FVector& InLastKnown)
{
	AIState = EShadeAIState::Search;
	TargetActor = nullptr;

	// Stop patrol wait and reset patrol leg state
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitHandle);
	bPatrolLegActive = false;
	CurrentGoalIndex = INDEX_NONE;

	StopMovement();

	if (Shade)
	{
		Shade->SetShadeState(EShadeState::Patrol); // use patrol anims
		Shade->SetAggressionFactor(0.2f);          // slightly faster than patrol
	}

	// Move to last known location and start search timer
	LastKnownPos = InLastKnown;
	MoveToLocation(LastKnownPos, 125.f);

	GetWorld()->GetTimerManager().ClearTimer(SearchTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		SearchTimerHandle,
		this,
		&ASFW_ShadeAIController::EnterPatrol,
		SearchDuration,
		false
	);
}

void ASFW_ShadeAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!IsValid(Actor)) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// Cancel timers and chase immediately
		GetWorld()->GetTimerManager().ClearTimer(SearchTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(PatrolWaitHandle);
		EnterChase(Actor);
	}
	else
	{
		// Only enter search if the lost actor was our current target
		if (Actor == TargetActor)
		{
			EnterSearch(Stimulus.StimulusLocation);
		}
	}
}

void ASFW_ShadeAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (AIState == EShadeAIState::Chase)
	{
		// If we got here, the path finished; unless we still see the target, fall back to search
		if (!TargetActor) return; // already cleared
		EnterSearch(TargetActor->GetActorLocation());
		return;
	}

	if (AIState == EShadeAIState::Search)
	{
		// Let the search timer handle returning to patrol
		return;
	}

	if (AIState == EShadeAIState::Patrol)
	{
		// Finish current patrol leg
		bPatrolLegActive = false;
		CurrentGoalIndex = INDEX_NONE;

		if (Result.Code == EPathFollowingResult::Success)
		{
			// Wait briefly at the point before picking the next one
			StartPatrolWait();
		}
		else
		{
			// Recovery: short fixed delay then try another point
			FTimerHandle Tmp;
			GetWorld()->GetTimerManager().SetTimer(Tmp, this, &ASFW_ShadeAIController::MoveToNextPatrolPoint, 1.f, false);
		}
		return;
	}
}

void ASFW_ShadeAIController::BuildPatrolPointList()
{
	PatrolPoints.Reset();
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATargetPoint::StaticClass(), PatrolPoints);
}

void ASFW_ShadeAIController::StartPatrol()
{
	if (PatrolPoints.Num() == 0) return;
	if (PatrolIndex == INDEX_NONE)
	{
		PatrolIndex = FMath::RandRange(0, PatrolPoints.Num() - 1);
	}
	MoveToNextPatrolPoint();
}

void ASFW_ShadeAIController::MoveToNextPatrolPoint()
{
	if (PatrolPoints.Num() == 0 || bPatrolLegActive) return;

	// choose next
	PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
	CurrentGoalIndex = PatrolIndex;

	// clear any pending waits
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitHandle);

	// start leg
	if (AActor* Dest = PatrolPoints[CurrentGoalIndex])
	{
		bPatrolLegActive = true;
		MoveToLocation(Dest->GetActorLocation(), /*AcceptanceRadius=*/180.f);
	}
}

void ASFW_ShadeAIController::StartPatrolWait()
{
	// If we're already en route, don't start another timer
	if (bPatrolLegActive) return;

	const float Delay = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
	GetWorld()->GetTimerManager().SetTimer(
		PatrolWaitHandle,
		this,
		&ASFW_ShadeAIController::MoveToNextPatrolPoint,
		Delay,
		false
	);
}

void ASFW_ShadeAIController::TryAttackTarget()
{
	if (!TargetActor || !GetPawn()) return;

	const float Dist = FVector::Dist(TargetActor->GetActorLocation(), GetPawn()->GetActorLocation());
	if (Dist <= AttackDistance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Shade reached target — would DOWN the player here."));
		// Attack handling will be wired later
	}
}


