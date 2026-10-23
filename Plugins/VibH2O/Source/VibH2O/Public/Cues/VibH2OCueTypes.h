// Cue types: what an input looks like once it has left its device, what a cue
// is, and the pure matching rules between the two.
//
// Everything here is free of engine state - no world, no subsystem, no device
// - so that the matching rules are tested on their own, outside any show.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/ObjectMacros.h"

#include "VibH2OCueTypes.generated.h"

class ALevelSequenceActor;

/** Where a cue input comes from. */
UENUM(BlueprintType)
enum class EVibH2OCueSource : uint8
{
	/** An OSC message - Resolume, the iPad, Max. Its address identifies it. */
	Osc UMETA(DisplayName = "OSC"),

	/** A MIDI Note On. Note Off is never a trigger: a pad release would fire twice. */
	MidiNote UMETA(DisplayName = "MIDI - Note"),

	/** A MIDI Control Change: a button, or a fader in continuous mode. */
	MidiControlChange UMETA(DisplayName = "MIDI - Control Change (CC)"),

	/** A MIDI Program Change. */
	MidiProgramChange UMETA(DisplayName = "MIDI - Program Change"),

	/**
	 * A key of the computer keyboard, read even when Unreal is in the
	 * background. Off unless enabled in the project settings: see
	 * docs/CUES.md for the risks.
	 */
	Key UMETA(DisplayName = "Touche du clavier")
};

/** What a cue does once triggered. */
UENUM(BlueprintType)
enum class EVibH2OCueAction : uint8
{
	/** Plays the sequence from where it stands. */
	PlaySequence UMETA(DisplayName = "Sequence - Lancer"),

	/** Rewinds the sequence to its start, then plays it. */
	PlaySequenceFromStart UMETA(DisplayName = "Sequence - Lancer depuis le debut"),

	PauseSequence UMETA(DisplayName = "Sequence - Pause"),

	/** Stops the sequence; the play head goes back to the start. */
	StopSequence UMETA(DisplayName = "Sequence - Arreter"),

	/** Jumps to a marked frame of the sequence, by its label. */
	JumpToMarker UMETA(DisplayName = "Sequence - Sauter a un marqueur"),

	/** Jumps to a time of the sequence, in seconds. */
	JumpToTime UMETA(DisplayName = "Sequence - Sauter a un temps"),

	/** Sets the tableau 1 to tableau 2 morph, through the /Blend route. */
	SetBlendAlpha UMETA(DisplayName = "Regler BlendAlpha"),

	/**
	 * Hands an OSC message to the plugin as if it had come off the network:
	 * /Attractor/<name>/, /Effect/<name>/, /Focus/... - every control route
	 * the plugin knows, without a line of cue code for each.
	 */
	SendOsc UMETA(DisplayName = "Message OSC interne"),

	/** Only raises OnCueFired, for a Blueprint to act on. */
	BlueprintEvent UMETA(DisplayName = "Evenement Blueprint seulement")
};

/**
 * One input, whatever its device, reduced to what a cue needs.
 *
 * The subsystem turns OSC control messages, MIDI messages and key presses
 * into these; the cue director only ever sees this form.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OCueInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	EVibH2OCueSource Source = EVibH2OCueSource::Osc;

	/** OSC address, exactly as received. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	FString Address;

	/** MIDI channel, 1 to 16. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	int32 Channel = 0;

	/** MIDI note, controller or program number, 0 to 127. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	int32 Number = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	FKey Key;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	bool bCtrl = false;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	bool bShift = false;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	bool bAlt = false;

	/**
	 * The input's value: the first numeric OSC argument as is, a MIDI velocity
	 * or controller value brought to 0..1. Meaningful only when bHasValue.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	float Value = 1.0f;

	/** False for an OSC message without a numeric argument, a program change, a key. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Cues")
	bool bHasValue = false;

	/** One line, in French, for the log and the learn status. */
	FString Describe() const;
};

/**
 * A cue: one trigger, one action.
 *
 * The trigger fields that do not apply to the chosen source are hidden in the
 * Details panel, and likewise for the action.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OCue
{
	GENERATED_BODY()

	/**
	 * Name shown in the log, and the one to use from OSC: "/Cue/<Name>" fires
	 * this cue whatever its own trigger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue", meta = (DisplayName = "Active"))
	bool bEnabled = true;

	// --------------------------------------------------------------- Trigger

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur", meta = (DisplayName = "Source"))
	EVibH2OCueSource Source = EVibH2OCueSource::Osc;

	/** Compared segment by segment, case and stray slashes ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Adresse OSC", EditCondition = "Source == EVibH2OCueSource::Osc", EditConditionHides))
	FString OscAddress;

	/** 1 to 16; 0 accepts every channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Canal MIDI (0 = tous)", ClampMin = "0", ClampMax = "16",
			EditCondition = "Source == EVibH2OCueSource::MidiNote || Source == EVibH2OCueSource::MidiControlChange || Source == EVibH2OCueSource::MidiProgramChange",
			EditConditionHides))
	int32 MidiChannel = 0;

	/** Note, controller or program number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Numero MIDI", ClampMin = "0", ClampMax = "127",
			EditCondition = "Source == EVibH2OCueSource::MidiNote || Source == EVibH2OCueSource::MidiControlChange || Source == EVibH2OCueSource::MidiProgramChange",
			EditConditionHides))
	int32 MidiNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Touche", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides))
	FKey Key;

	/**
	 * Modifiers the key requires. A bare key fires whatever application has
	 * the focus - typing a clip name in Resolume included - so a modifier is
	 * the safer choice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Avec Ctrl", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides))
	bool bRequireCtrl = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Avec Maj", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides))
	bool bRequireShift = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Avec Alt", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides))
	bool bRequireAlt = false;

	/**
	 * A valued input fires only at or above this value. Resolume sends 1 when
	 * a column is launched and 0 when the button is released; a MIDI button
	 * sends 127 then 0 (1.0 then 0.0 here). 0.5 lets the press through and
	 * stops the release.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur",
		meta = (DisplayName = "Seuil", ClampMin = "0.0", EditCondition = "!bContinuous"))
	float Threshold = 0.5f;

	/**
	 * Fires on every message, whatever its value, with no anti-bounce: for a
	 * fader driving BlendAlpha or an attractor. The value is passed on to the
	 * action.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Declencheur", meta = (DisplayName = "Continu (fader)"))
	bool bContinuous = false;

	// ---------------------------------------------------------------- Action

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (DisplayName = "Action"))
	EVibH2OCueAction Action = EVibH2OCueAction::PlaySequence;

	/** A Level Sequence actor of this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Sequence",
			EditCondition = "Action == EVibH2OCueAction::PlaySequence || Action == EVibH2OCueAction::PlaySequenceFromStart || Action == EVibH2OCueAction::PauseSequence || Action == EVibH2OCueAction::StopSequence || Action == EVibH2OCueAction::JumpToMarker || Action == EVibH2OCueAction::JumpToTime",
			EditConditionHides))
	TObjectPtr<ALevelSequenceActor> Sequence = nullptr;

	/** Label of the marked frame, as typed in Sequencer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Marqueur", EditCondition = "Action == EVibH2OCueAction::JumpToMarker", EditConditionHides))
	FString MarkerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Temps", ForceUnits = "s", ClampMin = "0.0",
			EditCondition = "Action == EVibH2OCueAction::JumpToTime", EditConditionHides))
	float TimeSeconds = 0.0f;

	/** After the jump: play on, or hold still on the new frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Lancer apres le saut",
			EditCondition = "Action == EVibH2OCueAction::JumpToMarker || Action == EVibH2OCueAction::JumpToTime",
			EditConditionHides))
	bool bPlayAfterJump = true;

	/** Address handed to the plugin, e.g. "/Attractor/Centre". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Adresse OSC envoyee", EditCondition = "Action == EVibH2OCueAction::SendOsc", EditConditionHides))
	FString OutOscAddress;

	/** The value sent - BlendAlpha for that action, the argument for an OSC message. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Valeur",
			EditCondition = "(Action == EVibH2OCueAction::SetBlendAlpha || Action == EVibH2OCueAction::SendOsc) && !bUseInputValue",
			EditConditionHides))
	float OutValue = 1.0f;

	/** Send the incoming value instead - for a fader in continuous mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Transmettre la valeur recue",
			EditCondition = "Action == EVibH2OCueAction::SetBlendAlpha || Action == EVibH2OCueAction::SendOsc",
			EditConditionHides))
	bool bUseInputValue = false;
};

namespace VibH2OCues
{
	/** Equal addresses, segment by segment: "/Cue/A/" equals "cue/a". */
	VIBH2O_API bool OscAddressesEqual(const FString& A, const FString& B);

	/** Does this input come from the cue's trigger? Ignores the value. */
	VIBH2O_API bool MatchesTrigger(const FVibH2OCue& Cue, const FVibH2OCueInput& Input);

	/** Does the input's value let the cue fire? Always true in continuous mode. */
	VIBH2O_API bool PassesThreshold(const FVibH2OCue& Cue, const FVibH2OCueInput& Input);

	/** Copies the input's identity into the cue's trigger - the learn step. */
	VIBH2O_API void ApplyLearnedInput(FVibH2OCue& Cue, const FVibH2OCueInput& Input);

	/**
	 * Decodes a MIDI short message as WinMM packs it - status in the low byte,
	 * then data 1, then data 2. Returns false for anything that is not a
	 * trigger: Note Off, Note On at velocity 0 (a Note Off in disguise),
	 * aftertouch, pitch bend, system messages.
	 */
	VIBH2O_API bool DecodeMidiShortMessage(uint32 Packed, FVibH2OCueInput& OutInput);
}
