// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SFW_PowerLibrary.generated.h"

class USFW_LampControllerComponent;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogSFWPower, Log, All);

/** Site/room power effects for lamps. Safe to call from Blueprints. */
UCLASS()
class PROJECTSENTINELLABS_API USFW_PowerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Black out every lamp for Seconds. Server only has effect. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "SFW|Power")
	static void BlackoutSite(UObject* WorldContextObject, float Seconds = 5.f);

	/** Black out lamps with matching RoomId for Seconds. Server only has effect. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "SFW|Power")
	static void BlackoutRoom(UObject* WorldContextObject, FName RoomId, float Seconds = 5.f);

	/** Flicker lamps with matching RoomId for Seconds. Server only has effect. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "SFW|Power")
	static void FlickerRoom(UObject* WorldContextObject, FName RoomId, float Seconds = 3.f);

	/** Trigger a temporary EMF spike on all EMF devices. Server only has effect. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "SFW|Power")
	static void TriggerEMFBurst(UObject* WorldContextObject, int32 Level = 5, float Seconds = 4.f);

	/** Mark an actor as an EMF source for a limited time. Server only has effect. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "SFW|Power")
	static void MakeActorEMFSource(UObject* WorldContextObject, AActor* TargetActor, float Seconds = 10.f);

private:
	static UWorld* GetWorldChecked(UObject* WorldContextObject);
	static bool IsServer(UWorld* World);

	static void ForEachLamp(UWorld* World, TFunctionRef<void(USFW_LampControllerComponent*)> Fn);
	static void ForEachLampInRoom(UWorld* World, FName RoomId, TFunctionRef<void(USFW_LampControllerComponent*)> Fn);
};
