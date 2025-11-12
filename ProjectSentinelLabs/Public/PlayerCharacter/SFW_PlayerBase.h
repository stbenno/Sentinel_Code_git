#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/Interact/SFW_InteractionComponent.h"

#include "SFW_PlayerBase.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UInputMappingContext;

UCLASS()
class PROJECTSENTINELLABS_API ASFW_PlayerBase : public ACharacter
{
	GENERATED_BODY()

public:
	ASFW_PlayerBase();

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/* Local FP camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FirstPersonCamera;

	/* Local-only First-Person Arms (OwnerOnly)*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* Mesh1P;

	/* Sprint State (replicated, useful for remote anim BP if needed) */
	UPROPERTY(ReplicatedUsing = OnRep_WantsToSprint, BlueprintReadOnly, Category = "Movement")
	bool bWantsToSprint = false;

	/* Speeds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 650.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CrouchSpeed = 200.f;

	// Helper to set speed from current state (walk vs sprint).
	// Crouch uses MaxWalkSpeedCrouched automatically.
	void ApplyMovementSpeed();

	// RepNotify for bWantsToSprint
	UFUNCTION()
	void OnRep_WantsToSprint();

	// Server RPC to set sprint
	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bSprint);

	// Enhanced Input assets (assign in the BP child)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultIMC = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Move = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Look = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Sprint = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Crouch = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Equipment")
	UInputAction* IA_EquipSlot1 = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Equipment")
	UInputAction* IA_EquipSlot2 = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Equipment")
	UInputAction* IA_EquipSlot3 = nullptr;

	// New: place active item (REM-POD etc.)
	UPROPERTY(EditDefaultsOnly, Category = "Input|Equipment")
	UInputAction* IA_Place = nullptr;

	// Toggle headlamp
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_ToggleHeadLamp = nullptr;

	UFUNCTION()
	void HandleToggleHeadLamp(const FInputActionValue& Value);

	// ---- Interact / Use ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Interact = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Use = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Drop = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	TObjectPtr<USFW_InteractionComponent> Interaction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	float InteractRange = 250.f;

	UFUNCTION()
	void TryInteract();

	UFUNCTION()
	void UseStarted(const FInputActionValue& Value);

	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);

	UFUNCTION()
	void DropStarted(const FInputActionValue& Value);

	// New: handler for placing the active item
	void PlaceStarted(const FInputActionValue& Value);

	// ---- Voice Chat Input ----
	// Local / proximity push-to-talk
	UPROPERTY(EditDefaultsOnly, Category = "Input|Voice")
	UInputAction* IA_PTT_Local = nullptr;

	// Radio / walkie push-to-talk
	UPROPERTY(EditDefaultsOnly, Category = "Input|Voice")
	UInputAction* IA_PTT_Radio = nullptr;

	// Replicate whether we're actively transmitting on local proximity voice
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Voice")
	bool bIsLocalPTTActive = false;

	// Replicate whether we're actively transmitting on radio/walkie
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Voice")
	bool bIsRadioPTTActive = false;

	// Handlers for local (proximity) PTT
	void LocalPTT_Pressed(const FInputActionValue& Value);
	void LocalPTT_Released(const FInputActionValue& Value);

	// Handlers for radio (walkie) PTT
	void RadioPTT_Pressed(const FInputActionValue& Value);
	void RadioPTT_Released(const FInputActionValue& Value);

	// Server RPCs so the server knows our talk state
	UFUNCTION(Server, Reliable)
	void Server_SetLocalPTT(bool bActive);

	UFUNCTION(Server, Reliable)
	void Server_SetRadioPTT(bool bActive);

	// ---- Movement / Look helpers ----
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void SprintStarted(const FInputActionValue& Value);
	void SprintCompleted(const FInputActionValue& Value);

	// Crouch is just Crouch/UnCrouch; speed is handled by MaxWalkSpeedCrouched.
	void CrouchStarted(const FInputActionValue& Value) { Crouch(false); }
	void CrouchCompleted(const FInputActionValue& Value) { UnCrouch(false); }

	// ---- Equipment slot select ----
	void EquipSlot1(const FInputActionValue& Value);
	void EquipSlot2(const FInputActionValue& Value);
	void EquipSlot3(const FInputActionValue& Value);

	// visibility (1P vs 3P meshes etc)
	void UpdateMeshVisibility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USFW_EquipmentManagerComponent> EquipmentManager;
};
