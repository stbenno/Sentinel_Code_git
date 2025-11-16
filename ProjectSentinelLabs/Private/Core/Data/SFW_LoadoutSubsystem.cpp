// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Data/SFW_LoadoutSubsystem.h"
#include "Core/Data/SFW_LoadoutRules.h"
#include "Core/Data/SFW_ItemDef.h"


void USFW_LoadoutSubsystem::GetBaseItems(TArray<USFW_ItemDef*>& Out) const
{
	Out.Reset();
	if (!ActiveRules) return;

	for (const TSoftObjectPtr<USFW_ItemDef>& SoftDef : ActiveRules->BaseItems)
	{
		if (USFW_ItemDef* Def = SoftDef.LoadSynchronous())
		{
			Out.Add(Def);
		}
	}
}

bool USFW_LoadoutSubsystem::TrySetSelectedOptionals(const TArray<FSFW_SelectedOptional>& InNewSelection, FString& OutFailureReason)
{
	if (!ActiveRules)
	{
		OutFailureReason = TEXT("No loadout rules active.");
		return false;
	}

	// Resolve hard pointers and validate constraints
	TArray<USFW_ItemDef*> Resolved;
	Resolved.Reserve(InNewSelection.Num());

	for (const FSFW_SelectedOptional& Sel : InNewSelection)
	{
		USFW_ItemDef* Def = Sel.Item.LoadSynchronous();
		if (!Def)
		{
			OutFailureReason = TEXT("Invalid item in selection.");
			return false;
		}

		// Ensure it's in OptionalItems whitelist
		bool bInWhitelist = false;
		for (const TSoftObjectPtr<USFW_ItemDef>& Soft : ActiveRules->OptionalItems)
		{
			if (Soft.Get() == Def) { bInWhitelist = true; break; }
		}
		if (!bInWhitelist || !Def->bIsOptional)
		{
			OutFailureReason = FString::Printf(TEXT("Item '%s' is not selectable."), *Def->GetName());
			return false;
		}

		// Charges sanity for consumables
		if (Def->Kind == ESFWItemKind::Consumable)
		{
			const int32 MaxC = FMath::Max(0, Def->MaxCharges);
			const int32 Ch = FMath::Clamp(Sel.Charges, 0, MaxC);
			if (Ch != Sel.Charges)
			{
				OutFailureReason = FString::Printf(TEXT("Charges for '%s' out of range."), *Def->GetName());
				return false;
			}
		}
		else
		{
			// Ignore non-consumable charge numbers silently
		}

		Resolved.Add(Def);
	}

	// Apply rules (slots/weight)
	if (!ActiveRules->WouldSelectionBeValid(Resolved, OutFailureReason))
	{
		return false;
	}

	SelectedOptionals = InNewSelection;
	return true;
}

void USFW_LoadoutSubsystem::ClearSelection()
{
	SelectedOptionals.Reset();
}