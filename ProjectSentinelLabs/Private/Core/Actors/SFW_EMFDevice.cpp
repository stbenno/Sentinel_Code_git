// Fill out your copyright notice in the Description page of Project Settings.

// SFW_EMFDevice.cpp

#include "Core/Actors/SFW_EMFDevice.h"

#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"

#include "Core/Game/SFW_GameState.h"

ASFW_EMFDevice::ASFW_EMFDevice()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	// Root skeletal mesh comes from ASFW_EquippableBase (Mesh)

	EMFMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EMFMesh"));
	EMFMesh->SetupAttachment(GetRootComponent());
	EMFMesh->SetIsReplicated(true);

	// Let BP / defaults drive collision when in the world.
	// Do NOT force NoCollision here.
	// e.g. in BP set:
	//   CollisionEnabled = QueryAndPhysics
	//   Visibility = Block
	//   WorldStatic/WorldDynamic = Block

	HumAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudioComp"));
	HumAudioComp->SetupAttachment(EMFMesh);
	HumAudioComp->bAutoActivate = false;

	// Do not hide or disable collision here; that breaks level-placed pickups.
	// State transitions are handled in OnEquipped / OnDropped.
	// SetActorHiddenInGame(false);
	// SetActorEnableCollision(true);

	bIsActive = false;
	EMFLevel = 0;
	BurstLevel = 0;
	BurstEndTime = 0.f;
}

UPrimitiveComponent* ASFW_EMFDevice::GetPhysicsComponent() const
{
	// Use the static mesh body for drop physics
	return EMFMesh;
}

void ASFW_EMFDevice::BeginPlay()
{
	Super::BeginPlay();

	// Build LED dynamic MIDs on all instances (server and clients)
	CacheLEDMaterials();

	// Sync visuals/audio to initial replicated state
	ApplyActiveState();
	UpdateLEDVisuals();
}

void ASFW_EMFDevice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_EMFDevice, bIsActive);
	DOREPLIFETIME(ASFW_EMFDevice, EMFLevel);
}

void ASFW_EMFDevice::OnEquipped(ACharacter* NewOwnerChar)
{
	Super::OnEquipped(NewOwnerChar);  // attaches to socket, turns physics off, restores transform

	UE_LOG(LogTemp, Log, TEXT("[EMF] OnEquipped called. OwnerChar=%s"),
		NewOwnerChar ? *NewOwnerChar->GetName() : TEXT("NULL"));

	if (EMFMesh)
	{
		EMFMesh->SetVisibility(true, true);
	}

	// Ensure LEDs are cached if BeginPlay ran pre-attachment
	if (LEDMIDs.Num() == 0)
	{
		CacheLEDMaterials();
	}

	ApplyActiveState();
	UpdateLEDVisuals();
}


void ASFW_EMFDevice::OnUnequipped()
{
	// Hide visuals while stored but not dropped
	if (EMFMesh)
	{
		EMFMesh->SetVisibility(false, true);
	}

	if (HumAudioComp && HumAudioComp->IsPlaying())
	{
		HumAudioComp->Stop();
	}

	Super::OnUnequipped();
}

void ASFW_EMFDevice::OnDropped(const FVector& DropLocation, const FVector& TossVelocity)
{
	// Base handles physics / collision via GetPhysicsComponent (EMFMesh)
	Super::OnDropped(DropLocation, TossVelocity);

	// Dropped into the world should be visible
	if (EMFMesh)
	{
		EMFMesh->SetVisibility(true, true);
	}

	// Optional: no hum while dropped
	if (HumAudioComp && HumAudioComp->IsPlaying())
	{
		HumAudioComp->Stop();
	}
}

void ASFW_EMFDevice::PrimaryUse()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->IsPlayerControlled() || !OwnerChar->IsLocallyControlled())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EMF] PrimaryUse by %s on client (LocallyControlled=1)"), *GetName());

	Server_RequestToggle();
}

EHeldItemType ASFW_EMFDevice::GetAnimHeldType_Implementation() const
{
	return EHeldItemType::EMF;
}

void ASFW_EMFDevice::Server_RequestToggle_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("[EMF] Server_RequestToggle_Implementation. Pre bIsActive=%d"), bIsActive ? 1 : 0);

	SetActive(!bIsActive);

	UE_LOG(LogTemp, Log, TEXT("[EMF] Server_RequestToggle_Implementation. Post bIsActive=%d"), bIsActive ? 1 : 0);
}

void ASFW_EMFDevice::SetActive(bool bEnable)
{
	if (!HasAuthority())
	{
		Server_SetActive(bEnable);
		return;
	}

	bIsActive = bEnable;

	UE_LOG(LogTemp, Log, TEXT("[EMF] SetActive AUTH. Now bIsActive=%d"), bIsActive ? 1 : 0);

	ApplyActiveState();
	UpdateLEDVisuals();
}

void ASFW_EMFDevice::Server_SetActive_Implementation(bool bEnable)
{
	bIsActive = bEnable;

	UE_LOG(LogTemp, Log, TEXT("[EMF] Server_SetActive_Implementation. Now bIsActive=%d"), bIsActive ? 1 : 0);

	ApplyActiveState();
	UpdateLEDVisuals();
}

void ASFW_EMFDevice::OnRep_IsActive()
{
	UE_LOG(LogTemp, Log, TEXT("[EMF] OnRep_IsActive -> %d"), bIsActive ? 1 : 0);

	ApplyActiveState();
	UpdateLEDVisuals();
}

void ASFW_EMFDevice::Server_SetEMFLevel_Implementation(int32 NewLevel)
{
	EMFLevel = FMath::Clamp(NewLevel, 0, 5);
	UpdateLEDVisuals();
}

void ASFW_EMFDevice::OnRep_EMFLevel()
{
	UpdateLEDVisuals();
}

void ASFW_EMFDevice::ApplyActiveState()
{
	// 1. Hum / power SFX are for local owner only
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	const bool bLocalOwner =
		OwnerChar &&
		OwnerChar->IsPlayerControlled() &&
		OwnerChar->IsLocallyControlled();

	if (HumAudioComp)
	{
		if (bIsActive)
		{
			if (bLocalOwner && !HumAudioComp->IsPlaying())
			{
				HumAudioComp->Play();
			}

			if (!bLocalOwner && HumAudioComp->IsPlaying())
			{
				HumAudioComp->Stop();
			}
		}
		else
		{
			if (HumAudioComp->IsPlaying())
			{
				HumAudioComp->Stop();
			}
		}
	}

	if (bLocalOwner)
	{
		if (bIsActive && PowerOnSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PowerOnSFX, GetActorLocation());
		}
		else if (!bIsActive && PowerOffSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PowerOffSFX, GetActorLocation());
		}
	}

	// 2. Server scan timer mirrors sources + bursts into our replicated EMFLevel
	if (HasAuthority())
	{
		if (bIsActive)
		{
			if (!GetWorldTimerManager().IsTimerActive(ScanTimerHandle))
			{
				GetWorldTimerManager().SetTimer(
					ScanTimerHandle,
					this,
					&ASFW_EMFDevice::DoServerScanForEMF,
					0.2f,
					true,
					0.0f
				);
			}
		}
		else
		{
			GetWorldTimerManager().ClearTimer(ScanTimerHandle);

			BurstLevel = 0;
			BurstEndTime = 0.f;
			Server_SetEMFLevel(0);
		}
	}
}

void ASFW_EMFDevice::TriggerAnomalyBurst(int32 Level, float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}

	Level = FMath::Clamp(Level, 0, 5);
	Seconds = FMath::Max(0.f, Seconds);

	if (Level <= 0 || Seconds <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	BurstLevel = Level;
	BurstEndTime = World->GetTimeSeconds() + Seconds;

	UE_LOG(LogTemp, Log,
		TEXT("[EMF] TriggerAnomalyBurst Level=%d Sec=%.2f Dev=%s"),
		Level, Seconds, *GetName());
}

void ASFW_EMFDevice::DoServerScanForEMF()
{
	if (!HasAuthority()) return;
	if (!bIsActive) return;

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	const FVector Origin = OwnerChar ? OwnerChar->GetActorLocation() : GetActorLocation();

	float StrongestSignal = 0.f;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Test = *It;
		if (!IsValid(Test) || Test == this)
		{
			continue;
		}

		if (!Test->ActorHasTag(FName("EMF_Source")))
		{
			continue;
		}

		const float Dist = FVector::Dist(Origin, Test->GetActorLocation());
		if (Dist > this->ScanRadius)
		{
			continue;
		}

		const float Normalized = FMath::Clamp((this->ScanRadius - Dist) / this->ScanRadius, 0.f, 1.f);

		if (Normalized > StrongestSignal)
		{
			StrongestSignal = Normalized;
		}
	}

	int32 NewLevel = FMath::Clamp(FMath::RoundToInt(StrongestSignal * 4.f), 0, 4);

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	float BurstTimeRemaining = 0.f;
	if (BurstLevel > 0 && World)
	{
		if (Now < BurstEndTime)
		{
			BurstTimeRemaining = BurstEndTime - Now;
			NewLevel = FMath::Max(NewLevel, BurstLevel);
		}
		else
		{
			BurstLevel = 0;
			BurstEndTime = 0.f;
		}
	}

	ASFW_GameState* GSLocal = World
		? World->GetGameState<ASFW_GameState>()
		: nullptr;

	if (GSLocal &&
		GSLocal->bEvidenceWindowActive &&
		GSLocal->CurrentEvidenceType == 1) // 1 == EMF tool id
	{
		if (NewLevel < 5)
		{
			NewLevel = 5;
		}
	}

	if (NewLevel != EMFLevel)
	{
		Server_SetEMFLevel(NewLevel);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[EMF] DoServerScanForEMF tick. bIsActive=%d EMFLevel=%d New=%d BurstLevel=%d BurstTime=%.2f"),
		bIsActive ? 1 : 0,
		EMFLevel,
		NewLevel,
		BurstLevel,
		BurstTimeRemaining
	);
}

void ASFW_EMFDevice::CacheLEDMaterials()
{
	LEDMIDs.Empty();

	TArray<UActorComponent*> MeshChildren;
	GetComponents(UStaticMeshComponent::StaticClass(), MeshChildren);

	for (UActorComponent* C : MeshChildren)
	{
		UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
		if (!SM) continue;

		if (!SM->ComponentHasTag(FName("EMF_LED")))
		{
			continue;
		}

		const int32 MatCount = SM->GetNumMaterials();
		for (int32 MatIdx = 0; MatIdx < MatCount; ++MatIdx)
		{
			UMaterialInterface* BaseMat = SM->GetMaterial(MatIdx);
			if (!BaseMat) continue;

			UMaterialInstanceDynamic* MID = SM->CreateAndSetMaterialInstanceDynamic(MatIdx);
			if (!MID) continue;

			LEDMIDs.Add(MID);

			UE_LOG(LogTemp, Log, TEXT("[EMF] Cached LED MID from %s[%d] -> %s"),
				*SM->GetName(), MatIdx, *MID->GetName());

			MID->SetScalarParameterValue(FName("GlowPower"), 1.0f);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EMF] CacheLEDMaterials complete. LEDMIDs.Num=%d"), LEDMIDs.Num());
}

void ASFW_EMFDevice::UpdateLEDVisuals()
{
	const int32 VisibleLevel = bIsActive ? EMFLevel : 0;

	for (int32 i = 0; i < LEDMIDs.Num(); ++i)
	{
		UMaterialInstanceDynamic* MID = LEDMIDs[i];
		if (!MID)
		{
			continue;
		}

		bool bLit = false;

		if (bIsActive)
		{
			if (i == 0)
			{
				bLit = true; // power indicator
			}
			else
			{
				bLit = (i < VisibleLevel);
			}
		}
		else
		{
			bLit = false;
		}

		const float GlowValue = bLit ? 200.0f : 0.1f;
		MID->SetScalarParameterValue(TEXT("GlowPower"), GlowValue);
	}
}
