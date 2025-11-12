// SFW_SigilActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFW_SigilActor.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_SigilActor : public AActor
{
	GENERATED_BODY()

public:
	ASFW_SigilActor();

	virtual void Tick(float DeltaTime) override;

	/** Mark this sigil as part of the current puzzle and "real". */
	void ActivateAsReal();

	/** Mark this sigil as part of the current puzzle but "fake". */
	void ActivateAsFake();

	/** Clear all state for a new round. */
	void ResetSigil();

	/** Is this sigil in the active layout for this round? */
	bool IsActive() const { return bIsActive; }

	/** Is this one of the "real" sigils? */
	bool IsReal() const { return bIsReal; }

	/**
	 * Called by the UV light on the SERVER every scan tick
	 * while the beam is hitting this sigil.
	 * DeltaSeconds is the scan interval.
	 */
	void NotifyUVHit(float DeltaSeconds);

	/**
	 * Server-side: force this sigil to be visible for ExtraSeconds
	 * (clamped by MaxVisibleSeconds). Also multicasts to clients.
	 */
	void ExposeByUV(float ExtraSeconds);

protected:
	virtual void BeginPlay() override;

	// ---------- Components ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sigil")
	TObjectPtr<UDecalComponent> SigilDecal;

	// ---------- Config ----------

	/** Scalar parameter in the decal material controlling opacity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sigil")
	FName OpacityParamName = TEXT("Sigil_Opacity");

	/** Max time a sigil can stay visible after being exposed by UV. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sigil")
	float MaxVisibleSeconds = 90.f;

	/** How long the fade-out takes at the end of visibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sigil")
	float FadeOutSeconds = 5.f;

	/** Continuous UV time required to fully reveal the sigil. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sigil|UV")
	float UVChargeSeconds = 3.f;

	/** Gap after which partial charge is discarded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sigil|UV")
	float UVChargeResetSeconds = 0.5f;

	// ---------- Runtime state ----------

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SigilMID;

	UPROPERTY(Transient)
	bool bIsActive = false;

	UPROPERTY(Transient)
	bool bIsReal = false;

	UPROPERTY(Transient)
	bool bIsRevealed = false;

	UPROPERTY(Transient)
	float RevealTimeRemaining = 0.f;

	// UV charge tracking
	UPROPERTY(Transient)
	float UVAccumulatedTime = 0.f;

	UPROPERTY(Transient)
	float LastUVHitTime = 0.f;

	UPROPERTY(Transient)
	bool bHasBeenFullyRevealed = false;

	// ---------- Net ----------

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ExposeByUV(float Duration);

	// ---------- Helpers ----------

	void UpdateVisual(float DeltaTime);
};
