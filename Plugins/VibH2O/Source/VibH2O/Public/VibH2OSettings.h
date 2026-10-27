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
 *
 * Property names are the ini keys: never rename one. Display names, tooltips
 * and categories are free to change.
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
	UPROPERTY(config, EditAnywhere, Category = "Réseau",
		meta = (DisplayName = "Adresse d'écoute",
			ToolTip = "Interface réseau écoutée. 0.0.0.0 accepte tout, broadcast compris. Pris en compte au prochain Play (ou VibH2O.Restart)."))
	FString ListenAddress = TEXT("0.0.0.0");

	/** UDP listen port. The Max patch sends on 9002 by default. */
	UPROPERTY(config, EditAnywhere, Category = "Réseau",
		meta = (ClampMin = "1", ClampMax = "65535", DisplayName = "Port d'écoute OSC",
			ToolTip = "Port UDP sur lequel Max envoie, 9002 par défaut. Pris en compte au prochain Play (ou VibH2O.Restart)."))
	int32 ListenPort = 9002;

	/** Start listening as soon as the subsystem initialises. */
	UPROPERTY(config, EditAnywhere, Category = "Réseau",
		meta = (DisplayName = "Écouter dès le démarrage",
			ToolTip = "Ouvre le port OSC dès le lancement. Décoché : rien n'arrive de Max."))
	bool bAutoStartReceiver = true;

	/** Receive buffer size, in bytes. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Réseau",
		meta = (ClampMin = "4096", DisplayName = "Tampon de réception",
			ToolTip = "Taille du tampon de réception, en octets. À agrandir seulement si des messages se perdent pendant les rafales."))
	int32 ReceiveBufferSize = 1 << 20;

	/**
	 * Maximum number of messages waiting in the queue. Beyond that, the newest
	 * are dropped rather than letting memory run away should the game thread
	 * fall behind.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Réseau",
		meta = (ClampMin = "256", DisplayName = "File d'attente max",
			ToolTip = "Nombre maximal de messages en attente. Au-delà, les plus récents sont jetés, plutôt que de laisser la mémoire s'emballer si l'image prend du retard."))
	int32 MaxQueuedMessages = 65536;

	/** Log every message received. For diagnosis only. */
	UPROPERTY(config, EditAnywhere, Category = "Réseau",
		meta = (DisplayName = "Journaliser tout le trafic OSC",
			ToolTip = "Ne jamais laisser coché en représentation : remplit le log et ralentit."))
	bool bLogOscTraffic = false;

	// ------------------------------------------------------------- Addresses

	/** Room plan prefix. Patch textedit reads "/RoomMapping". */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : plan de salle", ToolTip = "Préfixe des messages du plan de salle envoyés par Max."))
	FString RoomMappingPrefix = TEXT("/RoomMapping");

	/** Segment carrying the column count, under the room prefix. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Segment : colonnes", ToolTip = "Segment qui porte le nombre de colonnes, sous l'adresse du plan de salle."))
	FString ColumnsSegment = TEXT("columns");

	/** Segment carrying the row count. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Segment : rangées", ToolTip = "Segment qui porte le nombre de rangées, sous l'adresse du plan de salle."))
	FString RowsSegment = TEXT("rows");

	/** Heart rate prefix. Patch textedit reads "/BPM". */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : BPM", ToolTip = "Préfixe des rythmes cardiaques envoyés par Max."))
	FString BpmPrefix = TEXT("/BPM");

	/** Normalised arousal prefix. Still to be confirmed against the patch. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : excitation", ToolTip = "Préfixe de l'excitation de chaque spectateur."))
	FString ExcitationPrefix = TEXT("/SD");

	/**
	 * Synchrony prefix. The textedit in VIBH2O_Mapping.maxpat reads
	 * "/Synchronie"; docs/OSC_PROTOCOL.md assumed "/Sync". This field is here
	 * so the question can be settled without recompiling.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : synchronie", ToolTip = "Préfixe de la synchronie collective."))
	FString SynchronyPrefix = TEXT("/Synchronie");

	/**
	 * Prefix for a real beat event. Wired but unused today: Max only sends
	 * BPM. The day it sends beats, there will be nothing to write.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : battement",
			ToolTip = "Préfixe d'un vrai battement envoyé par Max. Prévu mais inutilisé : Max n'envoie aujourd'hui que le BPM."))
	FString BeatPrefix = TEXT("/Beat");

	/** Effect toggle prefix: "/Effect/<Name>/ 0|1". */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : effets", ToolTip = "« /Effect/<nom>/ 0|1 » éteint ou allume un effet."))
	FString EffectPrefix = TEXT("/Effect");

	/** Address of the tableau 1 to tableau 2 morph. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : transition salle → banc",
			ToolTip = "Position de la transition entre le plan de salle (0) et le banc (1)."))
	FString BlendPrefix = TEXT("/Blend");

	/** Address selecting the focus group. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : groupe de focus", ToolTip = "Choisit le groupe de focus."))
	FString FocusGroupPrefix = TEXT("/Focus/Group");

	/** Address of the focus intensity. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : intensité du focus", ToolTip = "Règle l'intensité du focus."))
	FString FocusAmountPrefix = TEXT("/Focus/Amount");

	/**
	 * Address of the backdrop: "/Fond <presence> [seconds]". 1 brings the
	 * underwater environment back, 0 fades it to pure black behind the
	 * bubbles; the optional second argument overrides the fade duration.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : fond",
			ToolTip = "« /Fond <présence> [secondes] » : 1 ramène l'environnement sous-marin, 0 le fond au noir derrière les bulles. Le second argument, facultatif, impose la durée du fondu."))
	FString BackdropPrefix = TEXT("/Fond");

	/**
	 * Address of the room mood: "/Ambiance auto|calme|normal|excite" forces a
	 * mood, or hands it back to the room's own excitation.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : ambiance",
			ToolTip = "« /Ambiance auto|calme|normal|excite » : force l'ambiance de la salle ou la rend automatique."))
	FString AmbiancePrefix = TEXT("/Ambiance");

	/**
	 * The control desk: "/Regie/<setting> value", "/Regie/Camera descente",
	 * "/Regie/Preset/Load <name>". The tablet's tabs, and any OSC sender.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : régie",
			ToolTip = "« /Regie/<réglage> valeur », « /Regie/Camera descente|arrivee|stop|depart|ensemble », « /Regie/Preset/Save|Load|Delete nom »."))
	FString RegiePrefix = TEXT("/Regie");

	/**
	 * Attractor prefix: "/Attractor/<name>/ 0..1" sets its activation,
	 * "/Attractor/<name>/Share/ 0..1" its share of the room.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : attracteurs",
			ToolTip = "« /Attractor/<nom>/ 0..1 » règle l'activation d'un attracteur, « /Attractor/<nom>/Share/ 0..1 » sa part de la salle."))
	FString AttractorPrefix = TEXT("/Attractor");

	/**
	 * Commands for every attractor at once: "/Attractors/Release/" lets them
	 * all go, "/Attractors/FadeScale/ x" scales every fade time.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : tous les attracteurs",
			ToolTip = "« /Attractors/Release/ » les relâche tous, « /Attractors/FadeScale/ x » multiplie toutes leurs durées de fondu."))
	FString AttractorsGlobalPrefix = TEXT("/Attractors");

	/**
	 * Exclusion zone prefix: "/Exclusion/<name>/ 0..1" sets its activation,
	 * "/Exclusion/<name>/Position/ x y" its live position, each from -1 to 1.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Adresses OSC",
		meta = (DisplayName = "Adresse : zones d'exclusion",
			ToolTip = "« /Exclusion/<nom>/ 0..1 » règle l'activation d'une zone, « /Exclusion/<nom>/Position/ x y » sa position en direct, de -1 à 1."))
	FString ExclusionPrefix = TEXT("/Exclusion");

	// ---------------------------------------------------------------- Remote

	/**
	 * Serve the iPad remote. The page is then at http://<this PC>:<port>/,
	 * the address being written to the log at start-up.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Télécommande iPad",
		meta = (DisplayName = "Activer la télécommande",
			ToolTip = "Sert la page de l'iPad, à l'adresse http://<ce PC>:<port>/. L'adresse exacte est écrite dans le log au démarrage."))
	bool bEnableRemote = true;

	/**
	 * TCP port of the remote. Away from 9002 on purpose: Max sends there, and
	 * the show's older Matrix_Controller_BP listens there too. Not 9080
	 * either, which Logitech G HUB holds on many PCs.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Télécommande iPad",
		meta = (ClampMin = "1", ClampMax = "65535", DisplayName = "Port",
			ToolTip = "Port TCP de la page iPad. Loin de 9002 exprès : Max envoie sur ce port. Éviter aussi 9080, souvent déjà pris."))
	int32 RemotePort = 8642;

	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Télécommande iPad",
		meta = (DisplayName = "Adresse d'écoute", ToolTip = "Interface réseau de la télécommande. 0.0.0.0 : toutes."))
	FString RemoteListenAddress = TEXT("0.0.0.0");

	/**
	 * Folder holding the remote's page. Empty: Resources/Remote inside the
	 * plugin. Point it elsewhere to try an edited page without touching the
	 * plugin.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Télécommande iPad",
		meta = (DisplayName = "Dossier de la page",
			ToolTip = "Dossier de la page iPad. Vide : celle fournie avec le plugin. Pointer ailleurs pour essayer une page modifiée sans toucher au plugin."))
	FString RemoteWebRoot;

	// ------------------------------------------------------------- Room plan

	/**
	 * Silence on the room addresses required before rebuilding the room. The
	 * plan arrives as a burst with no end marker: without this delay a 5 x 5
	 * room would be rebuilt 27 times in a row.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Plan de salle",
		meta = (ClampMin = "0.0", ClampMax = "2.0", ForceUnits = "s", DisplayName = "Silence avant de construire la salle",
			ToolTip = "Le plan arrive en rafale, sans marqueur de fin : la salle n'est construite qu'après ce silence. Sans lui, elle serait reconstruite à chaque message."))
	float RoomMappingSettleSeconds = 0.1f;

	/**
	 * Order in which the OSC index walks the seats. ColumnMajor is the value
	 * read from Scripts/FormatRoomMapping.js in the Max repository.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Plan de salle",
		meta = (DisplayName = "Ordre des sièges",
			ToolTip = "Ordre dans lequel le numéro OSC parcourt les sièges. « Colonnes d'abord » est celui de Max. Si la salle apparaît transposée, rangées et colonnes échangées, basculer ici."))
	EVibH2OSeatOrder SeatOrder = EVibH2OSeatOrder::ColumnMajor;

	/**
	 * Id values that mean "empty seat".
	 *
	 * 0 is confirmed by the real capture. 999 is added out of caution:
	 * Scripts/FormatRoomMapping.js substitutes 999 for an empty cell on one of
	 * its paths. Both are therefore read as "nobody here".
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Plan de salle",
		meta = (DisplayName = "ID signifiant « siège vide »",
			ToolTip = "Valeurs d'ID qui veulent dire « personne ici » : aucune bulle, et le siège n'entre dans aucune moyenne. 0 est confirmé ; 999 est ajouté par prudence."))
	TArray<int32> EmptySeatIds = { 0, 999 };

	/**
	 * Offset applied to the received id. Leave at 0 until the patch demands
	 * otherwise; the same Max script subtracts 3 on one of its paths, which
	 * remains to be confirmed in the venue.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Plan de salle",
		meta = (DisplayName = "Décalage des ID", ToolTip = "Décalage ajouté à l'ID reçu. Laisser à 0 tant que Max ne l'exige pas."))
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
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Plan de salle",
		meta = (DisplayName = "Siège absent = siège vide",
			ToolTip = "Décoché (conseillé) : un siège absent de la rafale est un paquet perdu, sa bulle reste. Coché : une bulle disparaîtrait à chaque paquet perdu, jusqu'à la rafale suivante."))
	bool bTreatMissingSeatsAsEmpty = false;

	// ------------------------------------------------------------------ Data

	/** Time without data after which a sensor is declared quiet. */
	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (ClampMin = "0.5", ForceUnits = "s", DisplayName = "Capteur muet après",
			ToolTip = "Sans donnée pendant ce temps, un capteur est déclaré muet."))
	float StaleTimeoutSeconds = 5.0f;

	/** Plausible BPM range. Anything outside it is ignored. */
	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (ClampMin = "1.0", ClampMax = "300.0", DisplayName = "BPM plausible : min",
			ToolTip = "Un BPM sous cette valeur est ignoré."))
	float MinBpm = 25.0f;

	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (ClampMin = "1.0", ClampMax = "300.0", DisplayName = "BPM plausible : max",
			ToolTip = "Un BPM au-dessus de cette valeur est ignoré."))
	float MaxBpm = 220.0f;

	/**
	 * Range of the arousal value as it arrives. Max says it is already
	 * normalised; these fields allow renormalising should the real range
	 * differ, without touching the code.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (DisplayName = "Excitation reçue : min",
			ToolTip = "Valeur d'excitation la plus basse envoyée par Max, ramenée à 0. Mettre -1 si Max envoie une excitation signée (0 = repos)."))
	float ExcitationInputMin = 0.0f;

	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (DisplayName = "Excitation reçue : max",
			ToolTip = "Valeur d'excitation la plus haute envoyée par Max, ramenée à 1."))
	float ExcitationInputMax = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (DisplayName = "Synchronie reçue : min",
			ToolTip = "Valeur de synchronie la plus basse envoyée par Max, ramenée à 0."))
	float SynchronyInputMin = 0.0f;

	UPROPERTY(config, EditAnywhere, Category = "Données",
		meta = (DisplayName = "Synchronie reçue : max",
			ToolTip = "Valeur de synchronie la plus haute envoyée par Max, ramenée à 1."))
	float SynchronyInputMax = 1.0f;

	/**
	 * Smoothing time constant for received values, in seconds. 0 disables
	 * smoothing. BPM is never smoothed: continuity is the phase's job, and
	 * smoothing the BPM would only delay the response.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Données",
		meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Lissage des valeurs",
			ToolTip = "Lissage des valeurs reçues, en secondes. 0 = aucun. Le BPM n'est jamais lissé : le battement reste continu sans cela."))
	float ValueSmoothingSeconds = 0.15f;

	/** Duration of the BeatPulse envelope after a beat, in seconds. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Données",
		meta = (ClampMin = "0.01", ForceUnits = "s", DisplayName = "Durée du pouls",
			ToolTip = "Durée du pouls (BeatPulse) après chaque battement, en secondes."))
	float BeatPulseDecaySeconds = 0.35f;

	// ------------------------------------------------------------------ Cues

	/**
	 * "/Cue/<name>" fires the cue of that name in every cue director of the
	 * level. The simplest route from Resolume: give a column a custom OSC
	 * address, nothing to learn.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Cues",
		meta = (DisplayName = "Adresse : cues",
			ToolTip = "« /Cue/<nom> » déclenche la cue de ce nom dans tous les directeurs de cues du niveau. Le plus simple depuis Resolume : donner à une colonne une adresse OSC personnalisée."))
	FString CuePrefix = TEXT("/Cue");

	UPROPERTY(config, EditAnywhere, Category = "Cues",
		meta = (DisplayName = "Lire le MIDI", ToolTip = "Ouvre les entrées MIDI choisies ci-dessous, pour les cues."))
	bool bEnableMidiInput = true;

	/**
	 * MIDI inputs to open, by part of their name, case ignored. "*" opens them
	 * all.
	 *
	 * Only "VibH2O" by default, that is a loopMIDI port named so: under
	 * classic Windows MIDI an input opened here is closed to every other
	 * application, and opening everything would take the hardware controller
	 * away from Resolume. A loopMIDI port, on the other hand, accepts several
	 * readers at once. See docs/CUES.md.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cues",
		meta = (DisplayName = "Entrées MIDI", EditCondition = "bEnableMidiInput",
			ToolTip = "Entrées MIDI à ouvrir, par morceau de nom, majuscules ignorées. « * » les ouvre toutes. Une entrée ouverte ici est fermée aux autres applications : ne pas prendre le contrôleur de Resolume, passer par un port loopMIDI (nommé VibH2O par défaut)."))
	TArray<FString> MidiInputDevices = { TEXT("VibH2O") };

	/**
	 * Read the keyboard even when Unreal is in the background.
	 *
	 * Off by default, on purpose: the key also reaches the application that
	 * has the focus, so a cue on a bare key fires whenever someone types in
	 * Resolume. Prefer MIDI or OSC; if keys are needed, give the cues a
	 * modifier. See docs/CUES.md.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cues",
		meta = (DisplayName = "Lire le clavier en arrière-plan",
			ToolTip = "Décoché par défaut, exprès : la touche arrive aussi à l'application qui a le focus, et une cue sur une touche seule partirait dès qu'on tape dans Resolume. Préférer le MIDI ou l'OSC, sinon donner un modificateur aux cues."))
	bool bEnableKeyboardCues = false;

	/**
	 * Keep the game at full speed when another application has the focus:
	 * resets t.IdleWhenNotForeground, lifts the editor's background throttle
	 * during Play, opts out of Windows power throttling. Without it the cues
	 * would arrive but the Spout feed could stutter.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cues",
		meta = (DisplayName = "Pleine vitesse en arrière-plan",
			ToolTip = "Garde le jeu à pleine vitesse quand une autre application, Resolume par exemple, a le focus. Sans cela, les cues arrivent mais le flux Spout peut saccader."))
	bool bKeepRunningInBackground = true;
};
