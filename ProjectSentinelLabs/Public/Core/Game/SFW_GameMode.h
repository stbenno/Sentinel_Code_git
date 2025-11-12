// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SFW_GameMode.generated.h"

class ASFW_AnomalyController;
class USFW_AgentCatalog;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_GameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ASFW_GameMode();

	UFUNCTION(BlueprintCallable)
	void StartRound();

	UFUNCTION(BlueprintCallable)
	void EndRound(bool bSuccess);

	UFUNCTION(BlueprintCallable)
	void FailRound(bool bSuccess);

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Class to spawn for the anomaly controller (set this to the BP in defaults). */
	UPROPERTY(EditDefaultsOnly, Category = "Anomaly")
	TSubclassOf<ASFW_AnomalyController> AnomalyControllerClass;

	/** Runtime instance of the anomaly controller. */
	UPROPERTY()
	ASFW_AnomalyController* AnomalyController = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agents")
	TObjectPtr<USFW_AgentCatalog> AgentCatalog = nullptr;
};
