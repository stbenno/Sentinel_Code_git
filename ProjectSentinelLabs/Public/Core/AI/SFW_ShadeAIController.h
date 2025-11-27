// SFW_ShadeAIController.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "SFW_ShadeAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class ARoomVolume;
class ASFW_ShadeCharacterBase;
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
	enum class EShadeAIState : uint8
	{
		TravelToRift,
		Patrol,
		Chase,
		Search
	};

protected:
	/** === State & Transitions === */
	EShadeAIState AIState = EShadeAIState::Patrol;

	// Early-game behavior – ignore players completely
	UPROPERTY(EditAnywhere, Category = "AI|Behavior")
	bool bIgnorePlayers = true;

	// Debug: draw Shade position + Rift target
	UPROPERTY(EditAnywhere, Category = "AI|Debug")
	bool bDebugDrawPath = true;

	void DebugDrawShade() const;

	// Enter-state helpers
	void EnterTravelToRift();
	void EnterPatrol();
	void EnterChase(AActor* NewTarget);
	void EnterSearch(const FVector& LastKnown);

	// Track last known player position during Search
	FVector LastKnownPos = FVector::ZeroVector;

	// Timed Search → Patrol
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

	/** === Rooms / travel targets === */
	UPROPERTY()
	ARoomVolume* BaseRoom = nullptr;

	UPROPERTY()
	ARoomVolume* RiftRoom = nullptr;

	// Center of the Base (used as initial “home”)
	UPROPERTY()
	FVector PatrolCenter = FVector::ZeroVector;

	// Center of the Rift (travel target)
	UPROPERTY()
	FVector RiftCenter = FVector::ZeroVector;

	// How close we need to get to RiftCenter before we consider it “reached”
	UPROPERTY(EditDefaultsOnly, Category = "AI|Travel")
	float TravelAcceptanceRadius = 200.f;

	// Travel retry handling
	FTimerHandle TravelRetryHandle;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Travel")
	float TravelRetryDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Travel")
	int32 MaxTravelRetries = 8;

	int32 TravelRetryCount = 0;

	// Door probe while traveling
	UPROPERTY(EditDefaultsOnly, Category = "AI|Travel")
	float DoorProbeDistance = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Travel")
	float DoorCheckInterval = 0.5f;

	double NextDoorCheckTime = 0.0;

	void TryResolveDoorBlockage();

	/** === Patrol === */
	// How far from the center the Shade can wander while patrolling
	UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
	float PatrolRadius = 800.f;

	FTimerHandle PatrolWaitHandle;

	// Add a short pause at each patrol leg
	UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
	float PatrolWaitMin = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
	float PatrolWaitMax = 5.0f;

	// Debounce: ensure only one patrol leg runs at a time
	bool bPatrolLegActive = false;
	int32 CurrentGoalIndex = INDEX_NONE;

	void StartPatrol();
	void MoveToNextPatrolPoint();
	void StartPatrolWait();

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
