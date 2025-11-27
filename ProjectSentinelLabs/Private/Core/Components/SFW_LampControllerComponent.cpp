// Fill out your copyright...

#include "Core/Components/SFW_LampControllerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/MeshComponent.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogLampCtrl, Log, All);

USFW_LampControllerComponent::USFW_LampControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USFW_LampControllerComponent::BeginPlay()
{
	Super::BeginPlay();
	CreateMIDsIfNeeded();
	ApplyState();
}

void USFW_LampControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USFW_LampControllerComponent, State);
}

void USFW_LampControllerComponent::OnRep_State()
{
	ApplyState();
}

void USFW_LampControllerComponent::SetState(ELampState NewState, float OptionalDurationSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	//UE_LOG(LogLampCtrl, Log, TEXT("[%s] SetState %d dur=%.2f"),
		//*GetOwner()->GetName(), (int32)NewState, OptionalDurationSeconds);

	State = NewState;
	OnRep_State();

	if (OptionalDurationSeconds > 0.f)
	{
		StartBlackoutRestoreTimer(OptionalDurationSeconds);
	}
}

void USFW_LampControllerComponent::StartBlackoutRestoreTimer(float DurationSeconds)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(RestoreTimer);
		W->GetTimerManager().SetTimer(
			RestoreTimer,
			[this]()
			{
				if (!GetOwner() || !GetOwner()->HasAuthority()) return;
				//UE_LOG(LogLampCtrl, Log, TEXT("[%s] Restore -> On"), *GetOwner()->GetName());
				State = ELampState::On;
				OnRep_State();
			},
			DurationSeconds, false);
	}
}

void USFW_LampControllerComponent::ApplyState()
{
	StopFlicker();

	switch (State)
	{
	case ELampState::On:
	{
		if (bUseMaterialSwap) ApplyMaterialOn();
		else                  ApplyEmissive(OnEmissive);
		ApplyLightForState(false);
		break;
	}
	case ELampState::Off:
	{
		if (bUseMaterialSwap) ApplyMaterialOff();
		else                  ApplyEmissive(OffEmissive);
		ApplyLightForState(true);
		break;
	}
	case ELampState::Flicker:
	{
		StartFlicker();
		break;
	}
	default: break;
	}
}

void USFW_LampControllerComponent::ApplyMaterialOn()
{
	if (UMeshComponent* M = ResolveMesh())
	{
		if (OnMaterial)
		{
			M->SetMaterial(MaterialIndex, OnMaterial);
			M->MarkRenderStateDirty();
			//UE_LOG(LogLampCtrl, Log, TEXT("[%s] MatSwap idx=%d -> %s"),
				//*GetOwner()->GetName(), MaterialIndex, *GetNameSafe(OnMaterial));
		}
	}
}

void USFW_LampControllerComponent::ApplyMaterialOff()
{
	if (UMeshComponent* M = ResolveMesh())
	{
		if (OffMaterial)
		{
			M->SetMaterial(MaterialIndex, OffMaterial);
			M->MarkRenderStateDirty();
			//UE_LOG(LogLampCtrl, Log, TEXT("[%s] MatSwap idx=%d -> %s"),
				//*GetOwner()->GetName(), MaterialIndex, *GetNameSafe(OffMaterial));
		}
	}
}

void USFW_LampControllerComponent::StartFlicker()
{
	// immediate tick for responsiveness
	TickFlickerOnce();
}

void USFW_LampControllerComponent::StopFlicker()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FlickerTimer);
	}
}

void USFW_LampControllerComponent::TickFlickerOnce()
{
	// 20% chance of full-dark pop
	const bool bPopOff = FMath::FRand() < 0.20f;

	float TargetEmissive = 0.f;
	if (bUseMaterialSwap)
	{
		// If using swap, emulate binary behaviour: pick On/Off
		if (bPopOff || bBinaryFlicker)
		{
			// exact off or exact on
			if (bPopOff) ApplyMaterialOff(); else ApplyMaterialOn();
			TargetEmissive = bPopOff ? 0.f : OnEmissive; // for logging symmetry
		}
		else
		{
			// analog look not supported with swap; just bias to On
			ApplyMaterialOn();
			TargetEmissive = OnEmissive;
		}
	}
	else
	{
		// Emissive path
		const float MaxE = FMath::Max(1.f, OnEmissive);

		if (bBinaryFlicker)
		{
			TargetEmissive = bPopOff ? 0.f : MaxE;
		}
		else
		{
			TargetEmissive = bPopOff ? 0.f : FMath::FRandRange(0.25f * MaxE, MaxE);
		}
		ApplyEmissive(TargetEmissive);
	}

	// Light: keep visible unless we’re snapping to off
	if (bControlLightIntensity)
	{
		if (ULightComponent* L = ResolveLight())
		{
			if (BaseLightIntensity < 0.f) BaseLightIntensity = L->Intensity;

			const bool bIsOffNow = (TargetEmissive <= OffSnapThreshold);
			const float Mult = bIsOffNow
				? 0.0f
				: (bBinaryFlicker ? 1.0f : FMath::FRandRange(0.25f, 1.0f));

			L->SetIntensity(BaseLightIntensity * Mult);
			L->SetVisibility(!bIsOffNow);
			//UE_LOG(LogLampCtrl, Log, TEXT("[%s] Flicker light mult=%.2f (base %.2f)"),
				//*GetOwner()->GetName(), Mult, BaseLightIntensity);
		}
	}

	const float Interval = FMath::FRandRange(FlickerIntervalMin, FlickerIntervalMax);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(FlickerTimer, this, &USFW_LampControllerComponent::TickFlickerOnce, Interval, false);
	}
}

void USFW_LampControllerComponent::ApplyEmissive(float Scalar)
{
	// hard snap near-zero to true 0 (for your "full stop must be 0" rule)
	if (Scalar <= OffSnapThreshold) Scalar = 0.f;

	CreateMIDsIfNeeded();
	for (UMaterialInstanceDynamic* MID : MIDs)
	{
		if (MID && EmissiveParamName != NAME_None)
		{
			MID->SetScalarParameterValue(EmissiveParamName, Scalar);
		}
	}

	/*UE_LOG(LogLampCtrl, Log, TEXT("[%s] Emissive %s = %.2f"),
		*GetOwner()->GetName(),
		*EmissiveParamName.ToString(),
		Scalar);    */
}

UMeshComponent* USFW_LampControllerComponent::ResolveMesh() const
{
	if (TargetMesh) return TargetMesh;

	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

ULightComponent* USFW_LampControllerComponent::ResolveLight()
{
	if (!bControlLightIntensity) return nullptr;
	if (TargetLight) return TargetLight;

	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	TArray<ULightComponent*> Lights;
	Owner->GetComponents<ULightComponent>(Lights);
	TargetLight = Lights.Num() > 0 ? Lights[0] : nullptr;
	return TargetLight;
}

void USFW_LampControllerComponent::ApplyLightForState(bool bForceOff)
{
	if (!bControlLightIntensity) return;

	if (ULightComponent* L = ResolveLight())
	{
		if (BaseLightIntensity < 0.f) BaseLightIntensity = L->Intensity;

		const bool bOff = bForceOff || State == ELampState::Off;
		const float Mult = bOff ? OffIntensityMultiplier : OnIntensityMultiplier;
		const float NewI = BaseLightIntensity * Mult;

		L->SetIntensity(NewI);
		L->SetVisibility(!bOff, true);

		//UE_LOG(LogLampCtrl, Log, TEXT("[%s] Light vis=%d intensity=%.2f (base %.2f x %.2f)"),
			//*GetOwner()->GetName(), !bOff, NewI, BaseLightIntensity, Mult);
	}
}

void USFW_LampControllerComponent::CreateMIDsIfNeeded()
{
	if (bUseMaterialSwap) return; // not needed for swap path
	if (MIDs.Num() > 0) return;

	if (UMeshComponent* M = ResolveMesh())
	{
		const int32 Count = M->GetNumMaterials();
		MIDs.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			if (M->GetMaterial(i))
			{
				if (UMaterialInstanceDynamic* MID = M->CreateAndSetMaterialInstanceDynamic(i))
				{
					MIDs.Add(MID);
				}
			}
		}
	}
}

void USFW_LampControllerComponent::ClearMIDs()
{
	MIDs.Reset();
}

void USFW_LampControllerComponent::RebuildMaterialInstances()
{
	ClearMIDs();
	CreateMIDsIfNeeded();
	ApplyState();
}








