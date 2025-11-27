// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Game/SFW_PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PlayerCharacter/Data/SFW_AgentCatalog.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

ASFW_PlayerState::ASFW_PlayerState() {}

void ASFW_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASFW_PlayerState, SelectedCharacterID);
	DOREPLIFETIME(ASFW_PlayerState, SelectedVariantID);
	DOREPLIFETIME(ASFW_PlayerState, bIsReady);
	DOREPLIFETIME(ASFW_PlayerState, bIsHost);

	DOREPLIFETIME(ASFW_PlayerState, AgentCatalog);
	DOREPLIFETIME(ASFW_PlayerState, CharacterIndex);

	// Loadout
	DOREPLIFETIME(ASFW_PlayerState, SelectedEquipmentIDs);

	// Sanity / anomaly
	DOREPLIFETIME(ASFW_PlayerState, Sanity);
	DOREPLIFETIME(ASFW_PlayerState, SanityTier);

	DOREPLIFETIME(ASFW_PlayerState, bInRiftRoom);
	DOREPLIFETIME(ASFW_PlayerState, bIsBlackedOut);
	DOREPLIFETIME(ASFW_PlayerState, BlackoutEndTime);

	DOREPLIFETIME(ASFW_PlayerState, bInSafeRoom);
}

void ASFW_PlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// 1 Hz server-side drift
		GetWorldTimerManager().SetTimer(
			SanityTickHandle,
			this,
			&ASFW_PlayerState::SanityTick,
			1.0f,
			true,
			1.0f
		);
	}
}

void ASFW_PlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(SanityTickHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------- Lobby ----------
void ASFW_PlayerState::SetSelectedCharacterAndVariant(const FName& InCharacterID, const FName& InVariantID)
{
	if (!HasAuthority()) return;

	if (SelectedCharacterID != InCharacterID)
	{
		SelectedCharacterID = InCharacterID;
		OnRep_SelectedCharacterID();
	}
	if (SelectedVariantID != InVariantID)
	{
		SelectedVariantID = InVariantID;
		OnRep_SelectedVariantID();
	}
}

void ASFW_PlayerState::SetIsReady(bool bNewReady)
{
	if (!HasAuthority()) return;
	if (bIsReady == bNewReady) return;

	bIsReady = bNewReady;
	OnRep_IsReady();
}

void ASFW_PlayerState::ResetForLobby()
{
	if (!HasAuthority()) return;

	if (bIsReady)
	{
		bIsReady = false;
		OnRep_IsReady();
	}

	if (SelectedEquipmentIDs.Num() > 0)
	{
		SelectedEquipmentIDs.Reset();
		OnRep_SelectedEquipmentIDs();
	}
}

void ASFW_PlayerState::ServerSetIsHost(bool bNewIsHost)
{
	check(HasAuthority());
	if (bIsHost == bNewIsHost) return;

	bIsHost = bNewIsHost;
	OnRep_IsHost();
}

void ASFW_PlayerState::OnRep_SelectedCharacterID()
{
	OnSelectedCharacterIDChanged.Broadcast(SelectedCharacterID);
}

void ASFW_PlayerState::OnRep_SelectedVariantID()
{
	OnSelectedVariantIDChanged.Broadcast(SelectedVariantID);
}

void ASFW_PlayerState::OnRep_IsReady()
{
	OnReadyChanged.Broadcast(bIsReady);
}

void ASFW_PlayerState::OnRep_IsHost()
{
	OnHostFlagChanged.Broadcast(bIsHost);
}

void ASFW_PlayerState::OnRep_CharacterIndex()
{
	ApplyIndexToSelectedID();
}

void ASFW_PlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	OnPlayerNameChanged.Broadcast(GetPlayerName());
}

// ---------- Equipment selection (server) ----------
void ASFW_PlayerState::SanitizeEquipmentArray(const TArray<FName>& InIDs, TArray<FName>& OutIDs) const
{
	OutIDs = InIDs;
	DedupAndStripNones(OutIDs);
}

void ASFW_PlayerState::DedupAndStripNones(TArray<FName>& InOutIDs) const
{
	TSet<FName> Seen;
	TArray<FName> Clean;
	Clean.Reserve(InOutIDs.Num());

	for (const FName ID : InOutIDs)
	{
		if (ID.IsNone()) continue;
		if (Seen.Contains(ID)) continue;
		Seen.Add(ID);
		Clean.Add(ID);
	}

	InOutIDs = MoveTemp(Clean);
}

void ASFW_PlayerState::ServerSetSelectedEquipment_Implementation(const TArray<FName>& NewEquipmentIDs)
{
	if (!HasAuthority()) return;

	TArray<FName> Clean;
	SanitizeEquipmentArray(NewEquipmentIDs, Clean);

	if (SelectedEquipmentIDs != Clean)
	{
		SelectedEquipmentIDs = MoveTemp(Clean);
		OnRep_SelectedEquipmentIDs();
	}
}

void ASFW_PlayerState::OnRep_SelectedEquipmentIDs()
{
	OnEquipmentSelectionChanged.Broadcast();
}

// ---------- Anomaly / Sanity ----------
void ASFW_PlayerState::ApplySanityDelta(float Delta)
{
	if (!HasAuthority()) return;

	const float Old = Sanity;
	Sanity = FMath::Clamp(Old + Delta, 0.f, 100.f);
	if (!FMath::IsNearlyEqual(Old, Sanity))
	{
		OnRep_Sanity();
		RecomputeAndApplySanityTier();
	}
}

void ASFW_PlayerState::SetSanity(float NewValue)
{
	if (!HasAuthority()) return;

	const float Old = Sanity;
	Sanity = FMath::Clamp(NewValue, 0.f, 100.f);
	if (!FMath::IsNearlyEqual(Old, Sanity))
	{
		OnRep_Sanity();
		RecomputeAndApplySanityTier();
	}
}

void ASFW_PlayerState::SetInRiftRoom(bool bIn)
{
	if (!HasAuthority()) return;
	if (bInRiftRoom == bIn) return;

	bInRiftRoom = bIn;
	OnRep_InRiftRoom();
}

void ASFW_PlayerState::StartBlackout(float DurationSeconds)
{
	if (!HasAuthority()) return;

	bIsBlackedOut = true;
	BlackoutEndTime = GetWorld()
		? GetWorld()->GetTimeSeconds() + FMath::Max(0.f, DurationSeconds)
		: 0.f;
	OnRep_Blackout();
}

void ASFW_PlayerState::ClearBlackout()
{
	if (!HasAuthority()) return;
	if (!bIsBlackedOut) return;

	bIsBlackedOut = false;
	BlackoutEndTime = 0.f;
	OnRep_Blackout();
}

void ASFW_PlayerState::SetInSafeRoom(bool bIn)
{
	if (!HasAuthority()) return;
	if (bInSafeRoom == bIn) return;

	bInSafeRoom = bIn;
	OnRep_SafeRoom();
}

void ASFW_PlayerState::OnRep_Sanity()
{
	OnSanityChanged.Broadcast(Sanity);
}

void ASFW_PlayerState::OnRep_SanityTier()
{
	OnSanityTierChanged.Broadcast(SanityTier);
}

void ASFW_PlayerState::OnRep_InRiftRoom()
{
	OnInRiftChanged.Broadcast(bInRiftRoom);
}

void ASFW_PlayerState::OnRep_Blackout()
{
	OnBlackoutChanged.Broadcast(bIsBlackedOut);
}

void ASFW_PlayerState::OnRep_SafeRoom()
{
	OnSafeRoomChanged.Broadcast();
}

// ---------- Agent helpers & RPCs ----------
int32 ASFW_PlayerState::GetAgentCount() const
{
	return AgentCatalog ? AgentCatalog->Agents.Num() : 0;
}

void ASFW_PlayerState::NormalizeIndex()
{
	const int32 Count = GetAgentCount();
	if (Count <= 0)
	{
		CharacterIndex = 0;
		return;
	}

	CharacterIndex = (CharacterIndex % Count + Count) % Count;
}

void ASFW_PlayerState::ApplyIndexToSelectedID()
{
	if (!AgentCatalog) return;

	const int32 Count = GetAgentCount();
	if (Count <= 0) return;
	if (!AgentCatalog->Agents.IsValidIndex(CharacterIndex)) return;

	const FName NewID = AgentCatalog->Agents[CharacterIndex].AgentID;
	if (SelectedCharacterID != NewID)
	{
		SelectedCharacterID = NewID;
		OnRep_SelectedCharacterID();
	}
}

int32 ASFW_PlayerState::FindIndexByAgentID(FName InCharacterID) const
{
	if (!AgentCatalog) return INDEX_NONE;

	for (int32 i = 0; i < AgentCatalog->Agents.Num(); ++i)
	{
		if (AgentCatalog->Agents[i].AgentID == InCharacterID)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void ASFW_PlayerState::ServerSetCharacterIndex_Implementation(int32 NewIndex)
{
	if (!HasAuthority()) return;

	CharacterIndex = NewIndex;
	NormalizeIndex();
	ApplyIndexToSelectedID();
}

void ASFW_PlayerState::ServerCycleCharacter_Implementation(int32 Direction)
{
	if (!HasAuthority()) return;
	if (GetAgentCount() <= 0) return;

	CharacterIndex += (Direction >= 0 ? 1 : -1);
	NormalizeIndex();
	ApplyIndexToSelectedID();
}

void ASFW_PlayerState::ServerSetCharacterByID_Implementation(FName InCharacterID)
{
	if (!HasAuthority()) return;

	const int32 Found = FindIndexByAgentID(InCharacterID);
	if (Found != INDEX_NONE)
	{
		CharacterIndex = Found;
		ApplyIndexToSelectedID();
		return;
	}

	if (SelectedCharacterID != InCharacterID)
	{
		SelectedCharacterID = InCharacterID;
		OnRep_SelectedCharacterID();
	}
}

void ASFW_PlayerState::ServerSetItemChecked_Implementation(FName ItemId, bool bIsChecked)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ItemId.IsNone())
	{
		return;
	}

	// Work on a copy first
	TArray<FName> Next = SelectedEquipmentIDs;

	if (bIsChecked)
	{
		// Add if not present
		if (!Next.Contains(ItemId))
		{
			Next.Add(ItemId);
		}
	}
	else
	{
		// Remove if present
		Next.Remove(ItemId);
	}

	// Clean up
	DedupAndStripNones(Next);

	// Only update + notify if something actually changed
	if (SelectedEquipmentIDs != Next)
	{
		SelectedEquipmentIDs = MoveTemp(Next);
		OnRep_SelectedEquipmentIDs();   // fire delegate locally on server
	}
}


FText ASFW_PlayerState::GetSelectedEquipmentSummary() const
{
	if (SelectedEquipmentIDs.Num() == 0)
	{
		return FText::FromString(TEXT("None"));
	}

	FString Summary;

	for (int32 i = 0; i < SelectedEquipmentIDs.Num(); ++i)
	{
		if (i > 0)
		{
			Summary += TEXT(", ");
		}

		Summary += SelectedEquipmentIDs[i].ToString();
	}

	return FText::FromString(Summary);
}



// ---------- Sanity tier compute (server) ----------
void ASFW_PlayerState::RecomputeAndApplySanityTier()
{
	check(HasAuthority());

	const float T1Min = SanityTier1Min;
	const float T2Min = SanityTier2Min;
	const float Hyst = FMath::Max(0.f, SanityHysteresis);

	ESanityTier NewTier = SanityTier;

	switch (SanityTier)
	{
	case ESanityTier::T1:
		if (Sanity < (T1Min - Hyst))
		{
			NewTier = ESanityTier::T2;
		}
		break;

	case ESanityTier::T2:
		if (Sanity >= T1Min)
		{
			NewTier = ESanityTier::T1;
		}
		else if (Sanity < (T2Min - Hyst))
		{
			NewTier = ESanityTier::T3;
		}
		break;

	case ESanityTier::T3:
		if (Sanity >= T2Min)
		{
			NewTier = ESanityTier::T2;
		}
		break;
	}

	if (NewTier != SanityTier)
	{
		SanityTier = NewTier;   // replicated
		OnRep_SanityTier();     // fire locally on server
	}
}

// ---------- Passive drift (server) ----------
void ASFW_PlayerState::SanityTick()
{
	if (!HasAuthority() || bIsBlackedOut) return;

	const float CeilValue = 100.f * FMath::Clamp(RecoveryCeilPct, 0.f, 1.f);
	float Delta = 0.f;

	if (bInSafeRoom)
	{
		if (Sanity < CeilValue)
		{
			Delta = SafeRoomRecoveryPerSec;
			if (Sanity + Delta > CeilValue)
			{
				Delta = CeilValue - Sanity;
			}
		}
	}
	else
	{
		Delta = -BaseDrainPerSec;

		// amplify drain near Shade
		if (Delta < 0.f && ShadeClass)
		{
			UWorld* W = GetWorld();
			APawn* MyPawn = nullptr;

			for (TActorIterator<APawn> ItPawn(W); ItPawn; ++ItPawn)
			{
				if (ItPawn->GetPlayerState() == this)
				{
					MyPawn = *ItPawn;
					break;
				}
			}

			if (MyPawn)
			{
				const float R2 = ShadeRadius * ShadeRadius;
				for (TActorIterator<AActor> ItShade(W, ShadeClass); ItShade; ++ItShade)
				{
					AActor* Shade = *ItShade;
					if (FVector::DistSquared(Shade->GetActorLocation(), MyPawn->GetActorLocation()) <= R2)
					{
						Delta *= ShadeDrainMultiplier;
						break;
					}
				}
			}
		}
	}

	if (!FMath::IsNearlyZero(Delta))
	{
		ApplySanityDelta(Delta);
	}
}
