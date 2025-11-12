// SFW_TPAnimInstance.cpp

#include "PlayerCharacter/Animation/SFW_TPAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Components/SFW_EquipmentManagerComponent.h"

USFW_TPAnimInstance::USFW_TPAnimInstance()
{
}

void USFW_TPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningPawn = TryGetPawnOwner();
	if (OwningPawn)
	{
		EquipComp = OwningPawn->FindComponentByClass<USFW_EquipmentManagerComponent>();
	}
}

void USFW_TPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Ensure we have a pawn
	if (!OwningPawn)
	{
		OwningPawn = TryGetPawnOwner();
	}

	if (!OwningPawn)
	{
		// No pawn, zero out key values
		Speed = 0.f;
		Direction = 0.f;
		bIsCrouched = false;
		AimPitch = 0.f;
		bIsHolding = false;
		bIsEquipping = false;
		bHasItemEquipped = false;
		HeldItemType = EHeldItemType::None;
		EquipState = EEquipState::Idle;
		return;
	}

	// ----- Movement data -----
	const FVector Vel = OwningPawn->GetVelocity();
	Speed = Vel.Size2D();

	const ACharacter* Char = Cast<ACharacter>(OwningPawn);
	const UCharacterMovementComponent* MoveComp = Char ? Char->GetCharacterMovement() : nullptr;
	bIsCrouched = MoveComp ? MoveComp->IsCrouching() : false;

	if (Speed > 3.f)
	{
		const FRotator YawRot(0.f, OwningPawn->GetActorRotation().Yaw, 0.f);
		const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Rt = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

		const float ForwardDot = FVector::DotProduct(Vel, Fwd);
		const float RightDot = FVector::DotProduct(Vel, Rt);

		Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	}
	else
	{
		Direction = 0.f;
	}

	// ----- Aim pitch for aim offset (works for local + remote) -----
	{
		const FRotator AimRot = OwningPawn->GetBaseAimRotation();   // uses controller or RemoteViewPitch
		const FRotator ActorRot = OwningPawn->GetActorRotation();

		const float RawPitch = (AimRot - ActorRot).GetNormalized().Pitch;
		AimPitch = FMath::Clamp(RawPitch, -90.f, 90.f);
	}

	// ----- Equipment state -----
	if (!EquipComp)
	{
		EquipComp = OwningPawn->FindComponentByClass<USFW_EquipmentManagerComponent>();
	}

	HeldItemType = EquipComp ? EquipComp->GetHeldItemType() : EHeldItemType::None;
	EquipState = EquipComp ? EquipComp->GetEquipState() : EEquipState::Idle;

	bIsEquipping = (EquipState == EEquipState::Equipping);
	bIsHolding = (EquipState == EEquipState::Holding);
	bHasItemEquipped =
		(HeldItemType != EHeldItemType::None) &&
		(EquipState == EEquipState::Holding ||
			EquipState == EEquipState::Equipping ||
			EquipState == EEquipState::Using);
}
