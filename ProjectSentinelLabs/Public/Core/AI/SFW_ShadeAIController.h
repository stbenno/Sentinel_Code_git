// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SFW_ShadeAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class ATargetPoint;
class ASFW_ShadeCharacterBase;
struct FAIStimulus;
struct FPathFollowingResult;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_ShadeAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASFW_ShadeAIController();

	// AAIController
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Simple controller-side state (no BT)
	enum class EShadeAIState : uint8 { Patrol, Chase, Search };

protected:
	/** === State & Transitions === */
	EShadeAIState AIState = EShadeAIState::Patrol;

	// Enter-state helpers
	void EnterPatrol();
	void EnterChase(AActor* NewTarget);
	void EnterSearch(const FVector& LastKnown);

	// Track last known player position during Search
	FVector LastKnownPos = FVector::ZeroVector;

	// Timed Search ? Patrol
	FTimerHandle SearchTimerHandle;

	// How long the Shade will investigate the last-known position before resuming Patrol
	UPROPERTY(EditDefaultsOnly, Category = "AI|Search")
	float SearchDuration = 6.0f;

	/** === Perception === */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAIPerceptionComponent* Perception = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	UAISenseConfig_Sight* SightConfig = nullptr;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** === Patrol === */
	UPROPERTY()
	TArray<AActor*> PatrolPoints;

	int32 PatrolIndex = INDEX_NONE;

	void BuildPatrolPointList();
	void StartPatrol();
	void MoveToNextPatrolPoint();

	// Add a short pause at each patrol point
	UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
	float PatrolWaitMin = 2.0f;   // you said 2–5s; set defaults here
	UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
	float PatrolWaitMax = 5.0f;

	FTimerHandle PatrolWaitHandle;
	void StartPatrolWait();

	// Debounce: ensure only one patrol leg runs at a time
	bool bPatrolLegActive = false;
	int32 CurrentGoalIndex = INDEX_NONE;

	/** === Target Handling === */
	UPROPERTY()
	AActor* TargetActor = nullptr;

	/** === Cached Pawn === */
	UPROPERTY()
	ASFW_ShadeCharacterBase* Shade = nullptr;

	/** === Interaction / Attack (stub) === */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat")
	float AttackDistance = 120.f;

	// Proximity check stub (wired later)
	void TryAttackTarget();
};