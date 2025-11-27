// SFW_UVLight.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Actors/SFW_EquippableBase.h"
#include "PlayerCharacter/Animation/SFW_EquipmentTypes.h"
#include "SFW_UVLight.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;
class USoundBase;
class UTextureLightProfile;
class UPrimitiveComponent;

/** Handheld UV light. */
UCLASS()
class PROJECTSENTINELLABS_API ASFW_UVLight : public ASFW_EquippableBase
{
	GENERATED_BODY()

public:
	ASFW_UVLight();

	// Equippable overrides
	virtual void OnEquipped(ACharacter* NewOwnerChar) override;
	virtual void OnUnequipped() override;
	virtual void PrimaryUse() override;

	// Anim type override
	virtual EHeldItemType GetAnimHeldType_Implementation() const override;

	// Toggle API
	UFUNCTION(BlueprintCallable, Category = "UVLight")
	void ToggleLight();

	UFUNCTION(BlueprintCallable, Category = "UVLight")
	void SetLightEnabled(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "UVLight")
	bool IsOn() const { return bIsOn; }

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UVLight")
	TObjectPtr<USpotLightComponent> Spot;

	// Photometric settings
	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	TObjectPtr<UTextureLightProfile> IESProfile = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	bool bUseIESBrightness = true;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	float IESBrightnessScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	ELightUnits IntensityUnits = ELightUnits::Lumens;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	float OnIntensity = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	float AttenuationRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	float InnerCone = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Light|Photometric")
	float OuterCone = 18.f;

	// Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_IsOn)
	bool bIsOn = false;

	// --- UV → Sigil reveal scan (server-side) ---

	/** How long a UV hit keeps a sigil visible (refreshed on re-hit). */
	UPROPERTY(EditDefaultsOnly, Category = "UVLight|Scan")
	float RevealSecondsPerHit = 90.f;

	/** Scan period while light is on. */
	UPROPERTY(EditDefaultsOnly, Category = "UVLight|Scan")
	float ScanInterval = 0.12f;

	/** Maximum distance (cm) to reveal sigils. */
	UPROPERTY(EditDefaultsOnly, Category = "UVLight|Scan")
	float RevealRange = 500.f;

	/** Half-angle of detection cone in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "UVLight|Scan")
	float ConeHalfAngleDeg = 8.f;

	/** Debug draw cone and hits. */
	UPROPERTY(EditDefaultsOnly, Category = "UVLight|Scan")
	bool bDebugDraw = false;

	FTimerHandle ScanTimer;

	// SFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UVLight|SFX")
	TObjectPtr<USoundBase> ToggleOnSFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UVLight|SFX")
	TObjectPtr<USoundBase> ToggleOffSFX = nullptr;

	// RepNotifies
	UFUNCTION()
	void OnRep_IsOn();

	// Server authority
	UFUNCTION(Server, Reliable)
	void Server_SetLightEnabled(bool bEnable);

	// Server scan tick
	void ServerScanTick();

	// Cosmetic sync
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayToggleSFX(bool bEnable);

	// Physics body
	virtual UPrimitiveComponent* GetPhysicsComponent() const override;

private:
	void ApplyLightState();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
