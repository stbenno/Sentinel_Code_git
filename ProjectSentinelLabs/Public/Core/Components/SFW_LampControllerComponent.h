#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFW_LampControllerComponent.generated.h"

class UMeshComponent;
class ULightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class ELampState : uint8
{
	On,
	Flicker,
	Off
};

/** Per-lamp controller. Replicates state, drives emissive or swaps materials, and modulates light. */
UCLASS(ClassGroup = (SFW), meta = (BlueprintSpawnableComponent))
class PROJECTSENTINELLABS_API USFW_LampControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFW_LampControllerComponent();

	/** Current state (server sets; clients receive). */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Lamp")
	ELampState State = ELampState::On;

	/** Set state (server only). Optional duration to auto-restore to On. */
	UFUNCTION(BlueprintCallable, Category = "Lamp")
	void SetState(ELampState NewState, float OptionalDurationSeconds = -1.f);

	/** Force re-scan of mesh materials and rebuild MIDs (emissive path). */
	UFUNCTION(BlueprintCallable, Category = "Lamp")
	void RebuildMaterialInstances();

	/** Lamp's logical room identifier (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Room")
	FName RoomId = NAME_None;

	// ---------- Visual Mode A: Material Swap ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Visual")
	bool bUseMaterialSwap = false; // default to emissive path

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Visual", meta = (EditCondition = "bUseMaterialSwap"))
	TObjectPtr<UMaterialInterface> OnMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Visual", meta = (EditCondition = "bUseMaterialSwap"))
	TObjectPtr<UMaterialInterface> OffMaterial = nullptr;

	// ---------- Visual Mode B: Emissive Scalar ----------
	/** Name of scalar parameter on the lamp material that feeds Emissive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Emissive", meta = (EditCondition = "!bUseMaterialSwap"))
	FName EmissiveParamName = TEXT("Emissive_Power");

	/** Emissive scalar when ON (your material expects ~240). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Emissive", meta = (EditCondition = "!bUseMaterialSwap"))
	float OnEmissive = 240.f;

	/** Emissive scalar when OFF (should be 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Emissive", meta = (EditCondition = "!bUseMaterialSwap"))
	float OffEmissive = 0.f;

	/** Any emissive <= this will be snapped to exactly 0 (to avoid "still on" look). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Emissive", meta = (EditCondition = "!bUseMaterialSwap"))
	float OffSnapThreshold = 1.0f;

	/** True = strictly 0 or full OnEmissive during flicker; False = analog 25..100% range plus occasional pops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Flicker")
	bool bBinaryFlicker = true;

	// ---------- Binding ----------
	/** Which mesh to drive (if null, uses first UMeshComponent on owner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Binding")
	TObjectPtr<UMeshComponent> TargetMesh = nullptr;

	/** Which material slot to swap/drive on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Binding")
	int32 MaterialIndex = 0;

	// ---------- Light Control ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Light")
	bool bControlLightIntensity = true;

	/** If null, auto-binds first ULightComponent on owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Light", meta = (EditCondition = "bControlLightIntensity"))
	TObjectPtr<ULightComponent> TargetLight = nullptr;

	/** Multiplier for light intensity when ON (1.0 keeps original). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Light", meta = (EditCondition = "bControlLightIntensity"))
	float OnIntensityMultiplier = 1.f;

	/** Multiplier for light intensity when OFF (0.0 = dark). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Light", meta = (EditCondition = "bControlLightIntensity"))
	float OffIntensityMultiplier = 0.f;

	// ---------- Flicker Timing ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Flicker")
	float FlickerIntervalMin = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp|Flicker")
	float FlickerIntervalMax = 0.20f;

protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_State();

private:
	// Material instances (emissive path)
	UPROPERTY(Transient) TArray<UMaterialInstanceDynamic*> MIDs;

	// Cached base light intensity (captured from the light the first time we touch it)
	UPROPERTY(Transient) float BaseLightIntensity = -1.f;

	FTimerHandle FlickerTimer;
	FTimerHandle RestoreTimer;

	// Core
	void ApplyState();
	void ApplyMaterialOn();
	void ApplyMaterialOff();
	void ApplyEmissive(float Scalar);
	void StartFlicker();
	void StopFlicker();
	void TickFlickerOnce();

	// Helpers
	UMeshComponent* ResolveMesh() const;
	ULightComponent* ResolveLight();
	void CreateMIDsIfNeeded();
	void ClearMIDs();
	void ApplyLightForState(bool bForceOff = false);
	void StartBlackoutRestoreTimer(float DurationSeconds);

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};