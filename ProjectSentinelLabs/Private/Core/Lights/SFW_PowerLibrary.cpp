// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Lights/SFW_PowerLibrary.h"
#include "Core/Components/SFW_LampControllerComponent.h"
#include "Core/Actors/SFW_EMFDevice.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogSFWPower);

UWorld* USFW_PowerLibrary::GetWorldChecked(UObject* WorldContextObject)
{
	check(WorldContextObject);
	UWorld* World = WorldContextObject->GetWorld();
	check(World);
	return World;
}

bool USFW_PowerLibrary::IsServer(UWorld* World)
{
	return World && (World->GetNetMode() != NM_Client);
}

void USFW_PowerLibrary::ForEachLamp(UWorld* World, TFunctionRef<void(USFW_LampControllerComponent*)> Fn)
{
	if (!World) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;

		if (USFW_LampControllerComponent* L = A->FindComponentByClass<USFW_LampControllerComponent>())
		{
			Fn(L);
		}
	}
}

void USFW_PowerLibrary::ForEachLampInRoom(UWorld* World, FName RoomId, TFunctionRef<void(USFW_LampControllerComponent*)> Fn)
{
	if (!World) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;

		if (USFW_LampControllerComponent* L = A->FindComponentByClass<USFW_LampControllerComponent>())
		{
			if (L->RoomId == RoomId)
			{
				Fn(L);
			}
		}
	}
}

void USFW_PowerLibrary::BlackoutSite(UObject* WorldContextObject, float Seconds)
{
	UWorld* W = GetWorldChecked(WorldContextObject);
	const int bAuth = IsServer(W) ? 1 : 0;

	int32 Total = 0;
	ForEachLamp(W, [&](USFW_LampControllerComponent* L)
		{
			++Total;
			if (bAuth) { L->SetState(ELampState::Off, Seconds); }
		});

	UE_LOG(LogSFWPower, Log, TEXT("[BlackoutSite] Sec=%.2f Auth=%d"), Seconds, bAuth);
	UE_LOG(LogSFWPower, Log, TEXT("[BlackoutSite] TotalLamps=%d"), Total);
}

void USFW_PowerLibrary::BlackoutRoom(UObject* WorldContextObject, FName RoomId, float Seconds)
{
	UWorld* W = GetWorldChecked(WorldContextObject);
	const int bAuth = IsServer(W) ? 1 : 0;

	int32 Total = 0, Matched = 0;
	ForEachLamp(W, [&](USFW_LampControllerComponent* L)
		{
			++Total;
			if (L->RoomId == RoomId)
			{
				++Matched;
				if (bAuth) { L->SetState(ELampState::Off, Seconds); }
			}
		});

	UE_LOG(LogSFWPower, Log,
		TEXT("[BlackoutRoom] Room=%s Sec=%.2f Auth=%d"),
		*RoomId.ToString(), Seconds, bAuth);
	UE_LOG(LogSFWPower, Log, TEXT("[BlackoutRoom] TotalLamps=%d Matched=%d"), Total, Matched);
}

void USFW_PowerLibrary::FlickerRoom(UObject* WorldContextObject, FName RoomId, float Seconds)
{
	UWorld* W = GetWorldChecked(WorldContextObject);
	const int bAuth = IsServer(W) ? 1 : 0;

	int32 Total = 0, Matched = 0;
	ForEachLamp(W, [&](USFW_LampControllerComponent* L)
		{
			++Total;
			if (L->RoomId == RoomId)
			{
				++Matched;
				if (bAuth) { L->SetState(ELampState::Flicker, Seconds); }
			}
		});

	UE_LOG(LogSFWPower, Log,
		TEXT("[FlickerRoom] Room=%s Sec=%.2f Auth=%d"),
		*RoomId.ToString(), Seconds, bAuth);
	UE_LOG(LogSFWPower, Log, TEXT("[FlickerRoom] TotalLamps=%d Matched=%d"), Total, Matched);
}

void USFW_PowerLibrary::TriggerEMFBurst(UObject* WorldContextObject, int32 Level, float Seconds)
{
	UWorld* W = GetWorldChecked(WorldContextObject);
	if (!IsServer(W))
	{
		return; // server authority only
	}

	Level = FMath::Clamp(Level, 0, 5);
	Seconds = FMath::Max(0.f, Seconds);

	if (Level <= 0 || Seconds <= 0.f)
	{
		UE_LOG(LogSFWPower, Verbose, TEXT("[EMFBurst] Ignored. Level=%d Sec=%.2f"), Level, Seconds);
		return;
	}

	int32 Count = 0;
	for (TActorIterator<ASFW_EMFDevice> It(W); It; ++It)
	{
		ASFW_EMFDevice* Dev = *It;
		if (!Dev) continue;

		Dev->TriggerAnomalyBurst(Level, Seconds);
		++Count;
	}

	UE_LOG(LogSFWPower, Log,
		TEXT("[EMFBurst] Level=%d Sec=%.2f Devices=%d"),
		Level, Seconds, Count);
}

void USFW_PowerLibrary::MakeActorEMFSource(UObject* WorldContextObject, AActor* TargetActor, float Seconds)
{
	if (!TargetActor)
	{
		return;
	}

	UWorld* W = GetWorldChecked(WorldContextObject);
	if (!IsServer(W))
	{
		// clients do nothing
		return;
	}

	static const FName EMFTag(TEXT("EMF_Source"));

	// Add tag if not already present
	if (!TargetActor->ActorHasTag(EMFTag))
	{
		TargetActor->Tags.Add(EMFTag);
	}

	UE_LOG(LogSFWPower, Log, TEXT("[MakeActorEMFSource] Actor=%s Sec=%.2f"),
		*TargetActor->GetName(), Seconds);

	// Seconds <= 0 means "leave it on" until something else clears it
	if (Seconds <= 0.f)
	{
		return;
	}

	TWeakObjectPtr<AActor> WeakTarget = TargetActor;

	FTimerDelegate RemoveDelegate;
	RemoveDelegate.BindLambda([WeakTarget]()
		{
			if (AActor* A = WeakTarget.Get())
			{
				static const FName EMFTagInner(TEXT("EMF_Source"));
				A->Tags.Remove(EMFTagInner);

				UE_LOG(LogSFWPower, Log, TEXT("[MakeActorEMFSource] Tag removed from %s"), *A->GetName());
			}
		});

	FTimerHandle TmpHandle;
	W->GetTimerManager().SetTimer(TmpHandle, RemoveDelegate, Seconds, false);
}
