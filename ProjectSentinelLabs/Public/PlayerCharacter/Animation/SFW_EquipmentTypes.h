#pragma once


#include "CoreMinimal.h"
#include "SFW_EquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EHeldItemType : uint8
{
	None         UMETA(DisplayName = "None"),

	// Light-style tools
	Flashlight   UMETA(DisplayName = "Flashlight"),
	UVLight      UMETA(DisplayName = "UV Light"),

	// Scan / meter tools
	EMF          UMETA(DisplayName = "EMF Meter"),
	Thermometer  UMETA(DisplayName = "Thermometer"),
	REMPod       UMETA(DisplayName = "REM-Pod"),
	SoundSensor  UMETA(DisplayName = "Sound Sensor"),
	Geiger       UMETA(DisplayName = "Geiger Counter"),

	// Other hand tools
	Camera       UMETA(DisplayName = "Camera"),
	WalkieTalkie UMETA(DisplayName = "Walkie Talkie"),

	// Fallback for future stuff
	GenericTool  UMETA(DisplayName = "Generic Tool")
};

UENUM(BlueprintType)
enum class EEquipState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Equipping   UMETA(DisplayName = "Equipping"),
    Holding     UMETA(DisplayName = "Holding"),
    Using       UMETA(DisplayName = "Using"),
    Unequipping UMETA(DisplayName = "Unequipping")
};