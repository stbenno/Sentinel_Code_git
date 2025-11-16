// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Data/SFW_LoadoutRules.h"
#include "Core/Data/SFW_ItemDef.h"

bool USFW_LoadoutRules::WouldSelectionBeValid(const TArray<USFW_ItemDef*>& Proposed, FString& OutFailureReason) const
{
	if (MaxOptionalSlots > 0 && Proposed.Num() > MaxOptionalSlots)
	{
		OutFailureReason = FString::Printf(TEXT("Too many optionals (%d > %d)."),
			Proposed.Num(), MaxOptionalSlots);
		return false;
	}

	if (MaxOptionalWeight > 0.f)
	{
		float Total = 0.f;
		for (const USFW_ItemDef* Def : Proposed)
		{
			if (!Def) continue;
			Total += FMath::Max(0.f, Def->Weight);
			if (Total > MaxOptionalWeight)
			{
				OutFailureReason = FString::Printf(TEXT("Over weight budget (%.1f > %.1f)."), Total, MaxOptionalWeight);
				return false;
			}
		}
	}

	return true;
}