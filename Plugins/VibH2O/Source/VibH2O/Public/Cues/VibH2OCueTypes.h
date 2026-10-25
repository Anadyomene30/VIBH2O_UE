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
	Key UMETA(DisplayName = "Clavier (arrière-plan)")
};

/** What a cue does once triggered. */
UENUM(BlueprintType)
enum class EVibH2OCueAction : uint8
{
	/** Plays the sequence from where it stands. */
	PlaySequence UMETA(DisplayName = "Séquence - Lancer"),

	/** Rewinds the sequence to its start, then plays it. */
	PlaySequenceFromStart UMETA(DisplayName = "Séquence - Lancer depuis le début"),

	PauseSequence UMETA(DisplayName = "Séquence - Pause"),

	/** Stops the sequence; the play head goes back to the start. */
	StopSequence UMETA(DisplayName = "Séquence - Arrêter"),

	/** Jumps to a marked frame of the sequence, by its label. */
	JumpToMarker UMETA(DisplayName = "Séquence - Sauter à un marqueur"),

	/** Jumps to a time of the sequence, in seconds. */
	JumpToTime UMETA(DisplayName = "Séquence - Sauter à un temps"),

	/** Sets the tableau 1 to tableau 2 morph, through the /Blend route. */
	SetBlendAlpha UMETA(DisplayName = "Tableau - Régler salle → banc"),

	/**
	 * Hands an OSC message to the plugin as if it had come off the network:
	 * /Attractor/<name>/, /Effect/<name>/, /Focus/... - every control route
	 * the plugin knows, without a line of cue code for each.
	 */
	SendOsc UMETA(DisplayName = "Message OSC interne"),

	/** Only raises OnCueFired, for a Blueprint to act on. */
	BlueprintEvent UMETA(DisplayName = "Événement Blueprint seulement")
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue",
		meta = (DisplayName = "Nom", ToolTip = "Nom écrit dans le log, et à utiliser en OSC : /Cue/<Nom> déclenche cette cue, quel que soit son déclencheur."))
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue",
		meta = (DisplayName = "Active", ToolTip = "Décochée : la cue ne se déclenche plus, ni par son déclencheur ni par /Cue/<Nom>."))
	bool bEnabled = true;

	// --------------------------------------------------------------- Trigger

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Source", ToolTip = "D'où vient l'entrée qui déclenche la cue : OSC (Resolume, iPad, Max), MIDI, ou clavier."))
	EVibH2OCueSource Source = EVibH2OCueSource::Osc;

	/** Compared segment by segment, case and stray slashes ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Adresse OSC", EditCondition = "Source == EVibH2OCueSource::Osc", EditConditionHides,
			ToolTip = "Adresse OSC attendue. Comparée segment par segment, sans tenir compte des majuscules ni des « / » en trop."))
	FString OscAddress;

	/** 1 to 16; 0 accepts every channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Canal MIDI (0 = tous)", ClampMin = "0", ClampMax = "16",
			ToolTip = "Canal MIDI, de 1 à 16. 0 accepte tous les canaux.",
			EditCondition = "Source == EVibH2OCueSource::MidiNote || Source == EVibH2OCueSource::MidiControlChange || Source == EVibH2OCueSource::MidiProgramChange",
			EditConditionHides))
	int32 MidiChannel = 0;

	/** Note, controller or program number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Numéro MIDI", ClampMin = "0", ClampMax = "127",
			ToolTip = "Numéro de note, de contrôleur (CC) ou de programme, de 0 à 127.",
			EditCondition = "Source == EVibH2OCueSource::MidiNote || Source == EVibH2OCueSource::MidiControlChange || Source == EVibH2OCueSource::MidiProgramChange",
			EditConditionHides))
	int32 MidiNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Touche", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides,
			ToolTip = "Touche lue même quand Unreal est en arrière-plan, si le clavier est activé dans les réglages du projet. Elle arrive aussi à l'application qui a le focus : préférer une touche avec Ctrl ou Alt."))
	FKey Key;

	/**
	 * Modifiers the key requires. A bare key fires whatever application has
	 * the focus - typing a clip name in Resolume included - so a modifier is
	 * the safer choice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Avec Ctrl", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides,
			ToolTip = "Une touche seule se déclenche aussi quand on tape un nom de clip dans Resolume : un modificateur est plus sûr."))
	bool bRequireCtrl = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Avec Maj", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides,
			ToolTip = "La touche ne déclenche la cue qu'avec Maj enfoncée."))
	bool bRequireShift = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Avec Alt", EditCondition = "Source == EVibH2OCueSource::Key", EditConditionHides,
			ToolTip = "La touche ne déclenche la cue qu'avec Alt enfoncée."))
	bool bRequireAlt = false;

	/**
	 * A valued input fires only at or above this value. Resolume sends 1 when
	 * a column is launched and 0 when the button is released; a MIDI button
	 * sends 127 then 0 (1.0 then 0.0 here). 0.5 lets the press through and
	 * stops the release.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Seuil", ClampMin = "0.0", UIMax = "1.0", EditCondition = "!bContinuous",
			ToolTip = "Une entrée avec valeur ne déclenche qu'à partir de ce seuil. Resolume et les pads envoient 1 à l'appui puis 0 au relâché : 0,5 laisse passer l'appui et arrête le relâché."))
	float Threshold = 0.5f;

	/**
	 * Fires on every message, whatever its value, with no anti-bounce: for a
	 * fader driving BlendAlpha or an attractor. The value is passed on to the
	 * action.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Déclencheur",
		meta = (DisplayName = "Continu (fader)",
			ToolTip = "Se déclenche à chaque message, quelle que soit sa valeur, sans anti-rebond : pour un fader qui pilote la transition ou un attracteur. La valeur est transmise à l'action."))
	bool bContinuous = false;

	// ---------------------------------------------------------------- Action

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Action", ToolTip = "Ce que fait la cue une fois déclenchée."))
	EVibH2OCueAction Action = EVibH2OCueAction::PlaySequence;

	/** A Level Sequence actor of this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Séquence", ToolTip = "Un acteur Level Sequence de ce niveau.",
			EditCondition = "Action == EVibH2OCueAction::PlaySequence || Action == EVibH2OCueAction::PlaySequenceFromStart || Action == EVibH2OCueAction::PauseSequence || Action == EVibH2OCueAction::StopSequence || Action == EVibH2OCueAction::JumpToMarker || Action == EVibH2OCueAction::JumpToTime",
			EditConditionHides))
	TObjectPtr<ALevelSequenceActor> Sequence = nullptr;

	/** Label of the marked frame, as typed in Sequencer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Marqueur", EditCondition = "Action == EVibH2OCueAction::JumpToMarker", EditConditionHides,
			ToolTip = "Nom du marqueur, tel qu'il est écrit dans Sequencer."))
	FString MarkerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Temps", ForceUnits = "s", ClampMin = "0.0",
			EditCondition = "Action == EVibH2OCueAction::JumpToTime", EditConditionHides,
			ToolTip = "Temps de la séquence où sauter, en secondes."))
	float TimeSeconds = 0.0f;

	/** After the jump: play on, or hold still on the new frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Lancer après le saut", ToolTip = "Coché : la séquence repart après le saut. Décoché : elle reste immobile sur la nouvelle image.",
			EditCondition = "Action == EVibH2OCueAction::JumpToMarker || Action == EVibH2OCueAction::JumpToTime",
			EditConditionHides))
	bool bPlayAfterJump = true;

	/** Address handed to the plugin, e.g. "/Attractor/Centre". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Adresse OSC envoyée", EditCondition = "Action == EVibH2OCueAction::SendOsc", EditConditionHides,
			ToolTip = "Exemples : /Attractor/HautGauche/ — /Effect/Beat/ — /Fond"))
	FString OutOscAddress;

	/** The value sent - BlendAlpha for that action, the argument for an OSC message. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Valeur", ToolTip = "Valeur envoyée : la position salle → banc (0 à 1) pour la transition, l'argument du message OSC sinon.",
			EditCondition = "(Action == EVibH2OCueAction::SetBlendAlpha || Action == EVibH2OCueAction::SendOsc) && !bUseInputValue",
			EditConditionHides))
	float OutValue = 1.0f;

	/** Send the incoming value instead - for a fader in continuous mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action",
		meta = (DisplayName = "Transmettre la valeur reçue", ToolTip = "Envoie la valeur de l'entrée à la place : pour un fader en mode continu.",
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
