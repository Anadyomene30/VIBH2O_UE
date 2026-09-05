// Project settings for the plugin. Anything that could change on the Max side
// without the code having to be revisited lives here.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VibH2OTypes.h"

#include "VibH2OSettings.generated.h"

/**
 * VibH2O settings, shown under Project Settings > Plugins > VibH2O and written
 * to Config/DefaultGame.ini.
 *
 * No OSC address is a compile-time constant. The protocol's remaining open
 * questions are therefore settled in the editor, not in a compiler.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "VibH2O"))
class VIBH2O_API UVibH2OSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVibH2OSettings();

	static const UVibH2OSettings* Get();

	virtual FName GetCategoryName() const override;

	// --------------------------------------------------------------- Network

	/** Listen interface. 0.0.0.0 accepts everything, broadcast included. */
	UPROPERTY(config, EditAnywhere, Category = "Reseau")
	FString ListenAddress = TEXT("0.0.0.0");

	/** UDP listen port. The Max patch sends on 9002 by default. */
	UPROPERTY(config, EditAnywhere, Category = "Reseau", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 ListenPort = 9002;

	/** Start listening as soon as the subsystem initialises. */
	UPROPERTY(config, EditAnywhere, Category = "Reseau")
	bool bAutoStartReceiver = true;

	/** Receive buffer size, in bytes. */
	UPROPERTY(config, EditAnywhere, Category = "Reseau", meta = (ClampMin = "4096"))
	int32 ReceiveBufferSize = 1 << 20;

	/**
	 * Maximum number of messages waiting in the queue. Beyond that, the newest
	 * are dropped rather than letting memory run away should the game thread
	 * fall behind.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Reseau", meta = (ClampMin = "256"))
	int32 MaxQueuedMessages = 65536;

	/** Log every message received. For diagnosis only. */
	UPROPERTY(config, EditAnywhere, Category = "Reseau")
	bool bLogOscTraffic = false;

	// ------------------------------------------------------------- Addresses

	/** Room plan prefix. Patch textedit reads "/RoomMapping". */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString RoomMappingPrefix = TEXT("/RoomMapping");

	/** Segment carrying the column count, under the room prefix. */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString ColumnsSegment = TEXT("columns");

	/** Segment carrying the row count. */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString RowsSegment = TEXT("rows");

	/** Heart rate prefix. Patch textedit reads "/BPM". */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString BpmPrefix = TEXT("/BPM");

	/** Normalised arousal prefix. Still to be confirmed against the patch. */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString ExcitationPrefix = TEXT("/SD");

	/**
	 * Synchrony prefix. The textedit in VIBH2O_Mapping.maxpat reads
	 * "/Synchronie"; docs/OSC_PROTOCOL.md assumed "/Sync". This field is here
	 * so the question can be settled without recompiling.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString SynchronyPrefix = TEXT("/Synchronie");

	/**
	 * Prefix for a real beat event. Wired but unused today: Max only sends
	 * BPM. The day it sends beats, there will be nothing to write.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString BeatPrefix = TEXT("/Beat");

	/** Effect toggle prefix: "/Effect/<Name>/ 0|1". */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString EffectPrefix = TEXT("/Effect");

	/** Address of the tableau 1 to tableau 2 morph. */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString BlendPrefix = TEXT("/Blend");

	/** Address selecting the focus group. */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString FocusGroupPrefix = TEXT("/Focus/Group");

	/** Address of the focus intensity. */
	UPROPERTY(config, EditAnywhere, Category = "Adresses OSC")
	FString FocusAmountPrefix = TEXT("/Focus/Amount");

	// ------------------------------------------------------------- Room plan

	/**
	 * Silence on the room addresses required before rebuilding the room. The
	 * plan arrives as a burst with no end marker: without this delay a 5 x 5
	 * room would be rebuilt 27 times in a row.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plan de salle", meta = (ClampMin = "0.0", ClampMax = "2.0", ForceUnits = "s"))
	float RoomMappingSettleSeconds = 0.1f;

	/**
	 * Order in which the OSC index walks the seats. ColumnMajor is the value
	 * read from Scripts/FormatRoomMapping.js in the Max repository.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plan de salle")
	EVibH2OSeatOrder SeatOrder = EVibH2OSeatOrder::ColumnMajor;

	/**
	 * Id values that mean "empty seat".
	 *
	 * 0 is confirmed by the real capture. 999 is added out of caution:
	 * Scripts/FormatRoomMapping.js substitutes 999 for an empty cell on one of
	 * its paths. Both are therefore read as "nobody here".
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plan de salle")
	TArray<int32> EmptySeatIds = { 0, 999 };

	/**
	 * Offset applied to the received id. Leave at 0 until the patch demands
	 * otherwise; the same Max script subtracts 3 on one of its paths, which
	 * remains to be confirmed in the venue.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plan de salle")
	int32 IndividualIdOffset = 0;

	/**
	 * Does a seat index missing from the burst mean an empty seat?
	 *
	 * False by default, and that is a robustness choice. The protocol always
	 * announces a free seat explicitly - the Ales preset sends 0 for its
	 * aisles, FormatRoomMapping.js sends 999. A missing index is therefore not
	 * information, it is silence: most often a lost UDP datagram. Reading it as
	 * "empty seat" would make a bubble vanish on every dropped packet until the
	 * next burst - a defect visible during a performance.
	 *
	 * Set to true should a patch one day come to mean erasure by omission.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plan de salle")
	bool bTreatMissingSeatsAsEmpty = false;

	// ------------------------------------------------------------------ Data

	/** Time without data after which a sensor is declared quiet. */
	UPROPERTY(config, EditAnywhere, Category = "Donnees", meta = (ClampMin = "0.5", ForceUnits = "s"))
	float StaleTimeoutSeconds = 5.0f;

	/** Plausible BPM range. Anything outside it is ignored. */
	UPROPERTY(config, EditAnywhere, Category = "Donnees", meta = (ClampMin = "1.0"))
	float MinBpm = 25.0f;

	UPROPERTY(config, EditAnywhere, Category = "Donnees", meta = (ClampMin = "1.0"))
	float MaxBpm = 220.0f;

	/**
	 * Range of the arousal value as it arrives. Max says it is already
	 * normalised; these fields allow renormalising should the real range
	 * differ, without touching the code.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Donnees")
	float ExcitationInputMin = 0.0f;

	UPROPERTY(config, EditAnywhere, Category = "Donnees")
	float ExcitationInputMax = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "Donnees")
	float SynchronyInputMin = 0.0f;

	UPROPERTY(config, EditAnywhere, Category = "Donnees")
	float SynchronyInputMax = 1.0f;

	/**
	 * Smoothing time constant for received values, in seconds. 0 disables
	 * smoothing. BPM is never smoothed: continuity is the phase's job, and
	 * smoothing the BPM would only delay the response.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Donnees", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ValueSmoothingSeconds = 0.15f;

	/** Duration of the BeatPulse envelope after a beat, in seconds. */
	UPROPERTY(config, EditAnywhere, Category = "Donnees", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float BeatPulseDecaySeconds = 0.35f;
};
