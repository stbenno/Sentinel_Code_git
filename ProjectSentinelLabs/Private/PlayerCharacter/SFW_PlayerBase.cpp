#include "PlayerCharacter/SFW_PlayerBase.h"
#include "Net/UnrealNetwork.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Controller.h"
#include "Core/Actors/Interface/SFW_InteractableInterface.h"
#include "Core/Components/SFW_EquipmentManagerComponent.h"
#include "Core/Interact/SFW_InteractionComponent.h"

ASFW_PlayerBase::ASFW_PlayerBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// FPS style rotation
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Movement speeds
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->NavAgentProps.bCanCrouch = true;

	bIsCrouched = false; // from ACharacter

	// Camera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));

	// 3P mesh (body others see)
	USkeletalMeshComponent* Mesh3P = GetMesh();
	Mesh3P->SetRelativeLocation(FVector(0.f, 0.f, -96.f)); // align to capsule
	Mesh3P->SetOwnerNoSee(true);
	Mesh3P->SetCastHiddenShadow(true);

	// 1P arms (local only)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P_Arms"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Gear + interact
	EquipmentManager = CreateDefaultSubobject<USFW_EquipmentManagerComponent>(TEXT("EquipmentManager"));
	Interaction = CreateDefaultSubobject<USFW_InteractionComponent>(TEXT("Interaction"));
}

void ASFW_PlayerBase::BeginPlay()
{
	Super::BeginPlay();

	UpdateMeshVisibility();

	// Init movement speed (walk vs sprint)
	ApplyMovementSpeed();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultIMC)
				{
					Subsys->AddMappingContext(DefaultIMC, /*Priority*/0);
				}
			}
		}
	}
}

void ASFW_PlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Movement axes
		if (IA_Move)  EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASFW_PlayerBase::Move);
		if (IA_Look)  EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASFW_PlayerBase::Look);

		// Sprint
		if (IA_Sprint)
		{
			EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ASFW_PlayerBase::SprintStarted);
			EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASFW_PlayerBase::SprintCompleted);
			EIC->BindAction(IA_Sprint, ETriggerEvent::Canceled, this, &ASFW_PlayerBase::SprintCompleted);
		}

		// Crouch (hold)
		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ASFW_PlayerBase::CrouchStarted);
			EIC->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &ASFW_PlayerBase::CrouchCompleted);
			EIC->BindAction(IA_Crouch, ETriggerEvent::Canceled, this, &ASFW_PlayerBase::CrouchCompleted);
		}

		// Interact
		if (IA_Interact)
		{
			EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ASFW_PlayerBase::TryInteract);
		}

		// Headlamp toggle
		if (IA_ToggleHeadLamp)
		{
			EIC->BindAction(IA_ToggleHeadLamp, ETriggerEvent::Started, this, &ASFW_PlayerBase::HandleToggleHeadLamp);
		}

		// Use active item
		if (IA_Use)
		{
			EIC->BindAction(IA_Use, ETriggerEvent::Started, this, &ASFW_PlayerBase::UseStarted);
		}

		// Drop active item
		if (IA_Drop)
		{
			EIC->BindAction(IA_Drop, ETriggerEvent::Started, this, &ASFW_PlayerBase::DropStarted);
		}

		// New: Place active item (REM-POD etc.)
		if (IA_Place)
		{
			EIC->BindAction(IA_Place, ETriggerEvent::Started, this, &ASFW_PlayerBase::PlaceStarted);
		}

		// Voice: Local PTT
		if (IA_PTT_Local)
		{
			EIC->BindAction(IA_PTT_Local, ETriggerEvent::Started, this, &ASFW_PlayerBase::LocalPTT_Pressed);
			EIC->BindAction(IA_PTT_Local, ETriggerEvent::Completed, this, &ASFW_PlayerBase::LocalPTT_Released);
			EIC->BindAction(IA_PTT_Local, ETriggerEvent::Canceled, this, &ASFW_PlayerBase::LocalPTT_Released);
		}

		// Voice: Radio PTT
		if (IA_PTT_Radio)
		{
			EIC->BindAction(IA_PTT_Radio, ETriggerEvent::Started, this, &ASFW_PlayerBase::RadioPTT_Pressed);
			EIC->BindAction(IA_PTT_Radio, ETriggerEvent::Completed, this, &ASFW_PlayerBase::RadioPTT_Released);
			EIC->BindAction(IA_PTT_Radio, ETriggerEvent::Canceled, this, &ASFW_PlayerBase::RadioPTT_Released);
		}

		// Equipment: select inventory slot 1/2/3
		if (IA_EquipSlot1)
		{
			EIC->BindAction(IA_EquipSlot1, ETriggerEvent::Started, this, &ASFW_PlayerBase::EquipSlot1);
		}
		if (IA_EquipSlot2)
		{
			EIC->BindAction(IA_EquipSlot2, ETriggerEvent::Started, this, &ASFW_PlayerBase::EquipSlot2);
		}
		if (IA_EquipSlot3)
		{
			EIC->BindAction(IA_EquipSlot3, ETriggerEvent::Started, this, &ASFW_PlayerBase::EquipSlot3);
		}
	}
}

void ASFW_PlayerBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UpdateMeshVisibility();
}

void ASFW_PlayerBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	UpdateMeshVisibility();
}

void ASFW_PlayerBase::ApplyMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move) return;

	// Only control stand/sprint speed here.
	// Crouched speed uses MaxWalkSpeedCrouched automatically.
	Move->MaxWalkSpeed = bWantsToSprint ? SprintSpeed : WalkSpeed;
}

void ASFW_PlayerBase::OnRep_WantsToSprint()
{
	ApplyMovementSpeed();
}

void ASFW_PlayerBase::ServerSetSprint_Implementation(bool bSprint)
{
	bWantsToSprint = bSprint;
	ApplyMovementSpeed();
}

void ASFW_PlayerBase::Move(const FInputActionValue& Value)
{
	const FVector2D Axis2D = Value.Get<FVector2D>();
	if (!Controller || Axis2D.IsNearlyZero()) return;

	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector  Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector  Rt = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Fwd, Axis2D.Y); // Y = forward/back
	AddMovementInput(Rt, Axis2D.X);  // X = right/left
}

void ASFW_PlayerBase::Look(const FInputActionValue& Value)
{
	const FVector2D Axis2D = Value.Get<FVector2D>();
	AddControllerYawInput(Axis2D.X);
	AddControllerPitchInput(Axis2D.Y);
}

void ASFW_PlayerBase::SprintStarted(const FInputActionValue& Value)
{
	bWantsToSprint = true;
	ApplyMovementSpeed();               // local prediction
	if (!HasAuthority()) ServerSetSprint(true);
}

void ASFW_PlayerBase::SprintCompleted(const FInputActionValue& Value)
{
	bWantsToSprint = false;
	ApplyMovementSpeed();               // local prediction
	if (!HasAuthority()) ServerSetSprint(false);
}

void ASFW_PlayerBase::EquipSlot1(const FInputActionValue& Value)
{
	if (EquipmentManager)
	{
		EquipmentManager->Server_EquipSlot(0);
	}
}

void ASFW_PlayerBase::EquipSlot2(const FInputActionValue& Value)
{
	if (EquipmentManager)
	{
		EquipmentManager->Server_EquipSlot(1);
	}
}

void ASFW_PlayerBase::EquipSlot3(const FInputActionValue& Value)
{
	if (EquipmentManager)
	{
		EquipmentManager->Server_EquipSlot(2);
	}
}

void ASFW_PlayerBase::UpdateMeshVisibility()
{
	const bool bLocal = IsLocallyControlled();

	if (Mesh1P)
	{
		Mesh1P->SetVisibility(bLocal, true);
		Mesh1P->SetOnlyOwnerSee(true);
	}
	if (USkeletalMeshComponent* Mesh3P = GetMesh())
	{
		// hide 3P mesh for self, show for others
		Mesh3P->SetOwnerNoSee(bLocal);
	}
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(bLocal);
	}
}

void ASFW_PlayerBase::Server_Interact_Implementation(AActor* Target)
{
	if (!IsValid(Target)) return;

	if (Target->GetClass()->ImplementsInterface(USFW_InteractableInterface::StaticClass()))
	{
		ISFW_InteractableInterface::Execute_Interact(Target, Controller); // server-side action
	}
}

void ASFW_PlayerBase::TryInteract()
{
	if (Interaction)
	{
		Interaction->TryInteract();
	}
}

void ASFW_PlayerBase::UseStarted(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("UseStarted"));
	if (auto* Equip = FindComponentByClass<USFW_EquipmentManagerComponent>())
	{
		Equip->UseActiveLocal();
	}
}

void ASFW_PlayerBase::DropStarted(const FInputActionValue& Value)
{
	if (EquipmentManager)
	{
		EquipmentManager->Server_DropActive();
	}
}

void ASFW_PlayerBase::PlaceStarted(const FInputActionValue& Value)
{
	if (EquipmentManager)
	{
		EquipmentManager->Server_PlaceActive();
	}
}

void ASFW_PlayerBase::HandleToggleHeadLamp(const FInputActionValue& Value)
{
	if (USFW_EquipmentManagerComponent* Mgr = FindComponentByClass<USFW_EquipmentManagerComponent>())
	{
		Mgr->Server_ToggleHeadLamp();
	}
}

// -------- Voice PTT handlers --------

void ASFW_PlayerBase::LocalPTT_Pressed(const FInputActionValue& Value)
{
	bIsLocalPTTActive = true;
	if (!HasAuthority())
	{
		Server_SetLocalPTT(true);
	}
}

void ASFW_PlayerBase::LocalPTT_Released(const FInputActionValue& Value)
{
	bIsLocalPTTActive = false;
	if (!HasAuthority())
	{
		Server_SetLocalPTT(false);
	}
}

void ASFW_PlayerBase::RadioPTT_Pressed(const FInputActionValue& Value)
{
	bool bCanRadio = false;
	if (EquipmentManager)
	{
		bCanRadio = EquipmentManager->CanUseRadioComms();
	}

	if (!bCanRadio)
	{
		return;
	}

	bIsRadioPTTActive = true;
	if (!HasAuthority())
	{
		Server_SetRadioPTT(true);
	}
}

void ASFW_PlayerBase::RadioPTT_Released(const FInputActionValue& Value)
{
	if (bIsRadioPTTActive)
	{
		bIsRadioPTTActive = false;
		if (!HasAuthority())
		{
			Server_SetRadioPTT(false);
		}
	}
}

void ASFW_PlayerBase::Server_SetLocalPTT_Implementation(bool bActive)
{
	bIsLocalPTTActive = bActive;
}

void ASFW_PlayerBase::Server_SetRadioPTT_Implementation(bool bActive)
{
	bIsRadioPTTActive = bActive;
}

void ASFW_PlayerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_PlayerBase, bWantsToSprint);
	DOREPLIFETIME(ASFW_PlayerBase, bIsLocalPTTActive);
	DOREPLIFETIME(ASFW_PlayerBase, bIsRadioPTTActive);
}
