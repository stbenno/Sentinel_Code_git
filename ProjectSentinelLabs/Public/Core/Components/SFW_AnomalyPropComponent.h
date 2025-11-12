// SFW_AnomalyPropComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFW_AnomalyPropComponent.generated.h"

/**
 * Component for props the anomaly can "manipulate":
 * - Temporarily shakes / bobs the actor (visual pulse)
 * - Marks the actor as an EMF source for the same time window
 * - Can apply a one-shot physics toss
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTSENTINELLABS_API USFW_AnomalyPropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFW_AnomalyPropComponent();

	/** Trigger a pulse. If PulseDurationSec <= 0, uses DefaultPulseDuration. */
	UFUNCTION(BlueprintCallable, Category = "Anomaly|Prop")
	void TriggerAnomalyPulse(float PulseDurationSec);

	/** One-shot soft toss using physics impulse. */
	UFUNCTION(BlueprintCallable, Category = "Anomaly|Prop")
	void TriggerAnomalyToss();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		enum ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

	/** Max vertical offset (in cm) while pulsing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Pulse")
	float MaxOffset = 5.f;

	/** Max extra yaw angle (in degrees) while pulsing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Pulse")
	float MaxAngleDeg = 8.f;

	/** Oscillation frequency in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Pulse")
	float PulseFrequency = 2.f;

	/** Default pulse duration if none is supplied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Pulse")
	float DefaultPulseDuration = 2.f;

	/**
	 * Extra seconds to keep EMF_Source active beyond visual pulse.
	 * Set to 0 to match exactly, or >0 to let EMF linger briefly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Pulse")
	float EMFSourceExtraSeconds = 0.25f;

	/** Min toss impulse strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Toss")
	float TossMinStrength = 200.f;

	/** Max toss impulse strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anomaly|Prop|Toss")
	float TossMaxStrength = 400.f;

private:
	bool bPulseActive = false;
	float PulseStartTime = 0.f;
	float PulseDuration = 0.f;

	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;

	void StopPulse();
};
