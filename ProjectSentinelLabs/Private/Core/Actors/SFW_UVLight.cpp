// SFW_UVLight.cpp

#include "Core/Actors/SFW_UVLight.h"


#include "Components/SpotLightComponent.h"
#include "Components/PrimitiveComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"

#include "Core/AnomalySystems/SFW_SigilActor.h"

ASFW_UVLight::ASFW_UVLight()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	EquipSlot = ESFWEquipSlot::Hand_Light;

	// UV spot
	Spot = CreateDefaultSubobject<USpotLightComponent>(TEXT("UVSpot"));
	Spot->SetupAttachment(Mesh);
	Spot->Mobility = EComponentMobility::Movable;
	Spot->IntensityUnits = IntensityUnits;
	Spot->bUseInverseSquaredFalloff = false;
	Spot->AttenuationRadius = AttenuationRadius;
	Spot->Intensity = 0.f;
	Spot->InnerConeAngle = InnerCone;
	Spot->OuterConeAngle = OuterCone;
	Spot->SetVisibility(false, true);

	bIsOn = false;

	// Default scan tuning (can be tweaked in BP)
	RevealRange = 500.f;
	ConeHalfAngleDeg = 8.f;
	ScanInterval = 0.12f;
	bDebugDraw = false;
}

void ASFW_UVLight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASFW_UVLight, bIsOn);
}



void ASFW_UVLight::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);

	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetEnableGravity(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(true, true);
	}

	ApplyLightState();
}

void ASFW_UVLight::OnUnequipped()
{
	if (Spot)
	{
		Spot->SetVisibility(false);
		Spot->SetIntensity(0.f);
	}

	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetEnableGravity(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(ScanTimer);
	}

	Super::OnUnequipped();
}

void ASFW_UVLight::PrimaryUse()
{
	ToggleLight();
}

EHeldItemType ASFW_UVLight::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::UVLight;
}

void ASFW_UVLight::ToggleLight()
{
	SetLightEnabled(!bIsOn);
}

void ASFW_UVLight::SetLightEnabled(bool bEnable)
{
	if (!HasAuthority())
	{
		Server_SetLightEnabled(bEnable);
		return;
	}

	bIsOn = bEnable;
	ApplyLightState();
	Multicast_PlayToggleSFX(bIsOn);
}

void ASFW_UVLight::Server_SetLightEnabled_Implementation(bool bEnable)
{
	bIsOn = bEnable;
	ApplyLightState();
	Multicast_PlayToggleSFX(bIsOn);
}

void ASFW_UVLight::OnRep_IsOn()
{
	ApplyLightState();
}

UPrimitiveComponent* ASFW_UVLight::GetPhysicsComponent() const
{
	return Mesh;
}

void ASFW_UVLight::ApplyLightState()
{
	const bool bVis = bIsOn;

	if (Spot)
	{
		Spot->SetVisibility(bVis);
		Spot->SetIntensity(bVis ? OnIntensity : 0.f);
		Spot->SetInnerConeAngle(InnerCone);
		Spot->SetOuterConeAngle(OuterCone);
		Spot->SetAttenuationRadius(AttenuationRadius);

		if (IESProfile)
		{
			Spot->SetIESTexture(IESProfile);
			Spot->bUseIESBrightness = bUseIESBrightness;
			Spot->IESBrightnessScale = IESBrightnessScale;
		}
	}

	if (HasAuthority())
	{
		if (bIsOn)
		{
			if (!GetWorldTimerManager().IsTimerActive(ScanTimer))
			{
				GetWorldTimerManager().SetTimer(
					ScanTimer,
					this,
					&ASFW_UVLight::ServerScanTick,
					ScanInterval,
					true,
					0.0f
				);
			}
		}
		else
		{
			GetWorldTimerManager().ClearTimer(ScanTimer);
		}
	}
}

void ASFW_UVLight::ServerScanTick()
{
	if (!HasAuthority() || !bIsOn)
	{
		return;
	}

	if (!Spot)
	{
		return;
	}

	const FVector Start = Spot->GetComponentLocation();
	const FVector Dir = Spot->GetForwardVector();

	const float CosThresh = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDeg));
	const float RangeSq = RevealRange * RevealRange;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, Start + Dir * RevealRange, FColor::Purple, false, ScanInterval, 0, 1.5f);
	}
#endif

	for (TActorIterator<ASFW_SigilActor> It(GetWorld()); It; ++It)
	{
		ASFW_SigilActor* Sigil = *It;
		if (!Sigil || !Sigil->IsActive())
		{
			continue;
		}

		const FVector To = Sigil->GetActorLocation() - Start;
		const float   DistSq = To.SizeSquared();
		if (DistSq > RangeSq)
		{
			continue;
		}

		const FVector ToN = To.GetSafeNormal();
		const float   Dot = FVector::DotProduct(Dir, ToN);
		if (Dot < CosThresh)
		{
			continue;
		}

		// Charge the sigil instead of instantly revealing it.
		Sigil->NotifyUVHit(ScanInterval);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bDebugDraw)
		{
			DrawDebugSphere(GetWorld(), Sigil->GetActorLocation(), 10.f, 8, FColor::Cyan, false, ScanInterval);
		}
#endif
	}
}

void ASFW_UVLight::Multicast_PlayToggleSFX_Implementation(bool bEnable)
{
	USoundBase* SFX = bEnable ? ToggleOnSFX : ToggleOffSFX;
	if (SFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SFX, GetActorLocation());
	}
}
