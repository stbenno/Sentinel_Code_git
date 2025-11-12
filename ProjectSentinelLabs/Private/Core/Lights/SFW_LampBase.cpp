// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Lights/SFW_LampBase.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Components/SFW_LampControllerComponent.h"
#include "Core/Rooms/RoomVolume.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	static FName ResolveRoomId(UWorld* World, const AActor* Actor)
	{
		if (!World || !Actor) return NAME_None;

		for (TActorIterator<ARoomVolume> It(World); It; ++It)
		{
			const ARoomVolume* Vol = *It;
			if (!Vol) continue;

			const UBoxComponent* Box = Cast<UBoxComponent>(Vol->GetCollisionComponent());
			if (Box && Box->IsOverlappingActor(Actor))
			{
				return Vol->RoomId;
			}
		}
		return NAME_None;
	}
}

ASFW_LampBase::ASFW_LampBase()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Lamp = CreateDefaultSubobject<USFW_LampControllerComponent>(TEXT("Lamp"));
}

void ASFW_LampBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Auto-fill from overlapping RoomVolume if not set in details
	if (RoomId.IsNone())
	{
		RoomId = ResolveRoomId(GetWorld(), this);
	}

	// Always push actor RoomId into the component (what PowerLibrary filters on)
	if (Lamp)
	{
		Lamp->RoomId = RoomId;

		// sensible default for emissive if not set per-instance
		if (Lamp->EmissiveParamName.IsNone())
		{
			Lamp->EmissiveParamName = TEXT("Glow");
		}
	}
}

#if WITH_EDITOR
void ASFW_LampBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Changed = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (Changed == GET_MEMBER_NAME_CHECKED(ASFW_LampBase, RoomId))
	{
		if (Lamp) { Lamp->RoomId = RoomId; }
	}
}
#endif
