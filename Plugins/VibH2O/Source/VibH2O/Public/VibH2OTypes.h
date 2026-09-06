// Shared VibH2O types: room model, per-individual state, focus groups.
// None of these know anything about the network or about rendering.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "VibH2OTypes.generated.h"

/**
 * Order in which the OSC seat index walks the room.
 *
 * See docs/OSC_PROTOCOL.md, pitfall 4. The correct value is ColumnMajor:
 * Scripts/FormatRoomMapping.js in the VibH2o repository computes
 * index = (x * rows + y) + 1, where x is the matrixctrl column. The setting
 * stays switchable because a Max patch can be edited without this code being
 * touched.
 */
UENUM(BlueprintType)
enum class EVibH2OSeatOrder : uint8
{
	/** The index runs along a row, then moves to the next one. */
	RowMajor UMETA(DisplayName = "Rangees d'abord (RowMajor)"),

	/** The index runs down a column, then moves to the next. Max's value. */
	ColumnMajor UMETA(DisplayName = "Colonnes d'abord (ColumnMajor)")
};

/** One seat of the room plan. */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OSeat
{
	GENERATED_BODY()

	/** OSC index, 1-based, exactly as received from Max. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 SeatIndex = 0;

	/** Column, 0-based, after SeatOrder has been applied. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 Column = 0;

	/** Row, 0-based, after SeatOrder has been applied. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 Row = 0;

	/** Id of whoever sits here. Check bOccupied before using it. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 IndividualId = 0;

	/**
	 * False for an empty seat. An empty seat gets no bubble, receives no data,
	 * and drops out of every average as well as out of the focus framing
	 * bounds - see pitfall 3 in the protocol.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	bool bOccupied = false;

	bool operator==(const FVibH2OSeat& Other) const
	{
		return SeatIndex == Other.SeatIndex
			&& Column == Other.Column
			&& Row == Other.Row
			&& IndividualId == Other.IndividualId
			&& bOccupied == Other.bOccupied;
	}
};

/**
 * A complete room plan. Rebuilt in one go at the end of a burst, then compared
 * against the previous one so only what actually moved is created or destroyed.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2ORoomSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 Columns = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 Rows = 0;

	/** Seats sorted by ascending OSC index, empty seats included. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	TArray<FVibH2OSeat> Seats;

	/** Generation counter, incremented on every rebuild. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Salle")
	int32 Generation = 0;

	bool IsValid() const { return Columns > 0 && Rows > 0 && Seats.Num() > 0; }

	int32 GetOccupiedCount() const
	{
		int32 Count = 0;
		for (const FVibH2OSeat& Seat : Seats)
		{
			Count += Seat.bOccupied ? 1 : 0;
		}
		return Count;
	}

	const FVibH2OSeat* FindSeatByIndex(int32 SeatIndex) const
	{
		for (const FVibH2OSeat& Seat : Seats)
		{
			if (Seat.SeatIndex == SeatIndex)
			{
				return &Seat;
			}
		}
		return nullptr;
	}

	void Reset()
	{
		Columns = 0;
		Rows = 0;
		Seats.Reset();
	}
};

/**
 * Live state of one individual.
 *
 * BeatPhase lives here, in the model, and not on the bubble. The reason is
 * structural: the heartbeat belongs to the person, not to the actor that
 * represents them. Were the phase carried by the bubble, the slightest room
 * rebuild - or the move from one tableau to the next - would reset it and make
 * the material's rings jump. Here it survives everything.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OIndividualState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	int32 IndividualId = 0;

	/** Raw heart rate, in beats per minute. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	float Bpm = 0.0f;

	/** Arousal, already normalised by Max. Unreal computes no baseline. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	float Excitation = 0.0f;

	/** How much this person is in sync with the group, 0 to 1. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	float Synchrony = 0.0f;

	/**
	 * Beat phase, 0 to 1, advancing by DeltaTime * (Bpm / 60) each frame. It
	 * never jumps: a change of BPM only changes how fast it advances.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	float BeatPhase = 0.0f;

	/** Decaying envelope since the last beat, 1 down to 0. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	float BeatPulse = 0.0f;

	/** Beats counted since this individual first appeared. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	int32 BeatCount = 0;

	/** True when the sensor has sent nothing for StaleTimeoutSeconds. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	bool bStale = false;

	/** Monotonic time of the last data received, in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	double LastUpdateTime = 0.0;

	/** True once any data has been received for this individual. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Individu")
	bool bEverReceived = false;

	/**
	 * Last raw values received, before smoothing. Not reflected: they are an
	 * implementation detail of the smoothing, not data worth exposing.
	 */
	float ExcitationTarget = 0.0f;
	float SynchronyTarget = 0.0f;

	/** True during the frame in which a beat was crossed. */
	bool bBeatThisFrame = false;

	/**
	 * True when this individual counts towards the averages: data received at
	 * least once, and the sensor not gone quiet.
	 */
	bool CountsTowardAverages() const { return bEverReceived && !bStale; }
};

/**
 * A focus group: a rectangular zone, plus a list of loose seats for the
 * scattered selections a rectangle cannot describe.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OFocusGroup
{
	GENERATED_BODY()

	/** Name quoted by OSC, by Blueprint, or in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus")
	FName GroupName = NAME_None;

	/** Take the rectangle below into account. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus")
	bool bUseRect = true;

	/** Rectangle bounds, in 0-based columns and rows, inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus", meta = (EditCondition = "bUseRect", ClampMin = "0"))
	int32 ColumnMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus", meta = (EditCondition = "bUseRect", ClampMin = "0"))
	int32 ColumnMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus", meta = (EditCondition = "bUseRect", ClampMin = "0"))
	int32 RowMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus", meta = (EditCondition = "bUseRect", ClampMin = "0"))
	int32 RowMax = 0;

	/** OSC indices of seats added to the group, outside the rectangle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus")
	TArray<int32> ExtraSeatIndices;

	/** Ids of individuals added to the group, wherever they are sitting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus")
	TArray<int32> ExtraIndividualIds;

	/** True when the given seat belongs to this group. */
	bool Contains(const FVibH2OSeat& Seat) const
	{
		if (bUseRect
			&& Seat.Column >= FMath::Min(ColumnMin, ColumnMax)
			&& Seat.Column <= FMath::Max(ColumnMin, ColumnMax)
			&& Seat.Row >= FMath::Min(RowMin, RowMax)
			&& Seat.Row <= FMath::Max(RowMin, RowMax))
		{
			return true;
		}
		if (ExtraSeatIndices.Contains(Seat.SeatIndex))
		{
			return true;
		}
		if (Seat.bOccupied && ExtraIndividualIds.Contains(Seat.IndividualId))
		{
			return true;
		}
		return false;
	}
};

/** State of a named effect. Scope is global. */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OEffectState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Effets")
	FName EffectName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Effets")
	bool bEnabled = true;
};

/**
 * Names of the effects wired into the plugin. Any other name is simply relayed
 * to Blueprints, which lets the artist define effects without touching C++.
 */
namespace VibH2OEffects
{
	/** Permanent drifting float of the bubbles. */
	VIBH2O_API extern const FName Float;

	/** Arousal-driven drift. */
	VIBH2O_API extern const FName Drift;

	/** Advance of the beat phase. */
	VIBH2O_API extern const FName Beat;

	/** Tint taken from the arousal gradient. */
	VIBH2O_API extern const FName Tint;

	/** Dimming of bubbles outside the focus group. */
	VIBH2O_API extern const FName Focus;

	/** Size variation driven by arousal. */
	VIBH2O_API extern const FName Scale;
}
