// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFW_InteractionComponent.generated.h"


class APlayerController;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTSENTINELLABS_API USFW_InteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFW_InteractionComponent();

	// Call from input binding on the owning pawn/controller
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractInput();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryUseOrInteract();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractRange = 250.f;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float TraceRateHz = 10.f;          // client-side focus update rate

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float DebounceSeconds = 0.15f;     // server spam guard

	UPROPERTY(EditAnywhere, Category = "Interaction|Debug")
	bool bDrawDebug = false;

private:
	// client-only focus cache
	UPROPERTY(Transient)
	AActor* FocusedActor = nullptr;

	float TraceAccumulator = 0.f;
	double LastServerInteractTime = -1.0;

	// ---- server RPC ----
	UFUNCTION(Server, Reliable)
	void Server_TryInteract(AActor* Target);

	// ---- helpers ----
	APlayerController* GetOwningPC() const;
	void UpdateFocus();
	void SetFocus(AActor* NewFocus);
	bool FindBestInteractable(AActor*& OutActor) const;

	bool ValidateServerInteract(AActor* Target, APlayerController* PC) const;
};