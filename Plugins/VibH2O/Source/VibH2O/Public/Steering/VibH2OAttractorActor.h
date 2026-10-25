// An attractor: a zone placed by hand in the level, toward which part of the
// room is drawn on cue.
//
// It owns no bubble and moves none itself. The stage asks it, bubble by
// bubble, how strongly it pulls and where to, AFTER the tableaux are blended -
// so the grid and the school never stop running underneath. Releasing an
// attractor therefore needs no memory of where anyone was: the bubbles simply
// fall back onto drivers that kept their place all along.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Steering/VibH2OSteeringMath.h"

#include "VibH2OAttractorActor.generated.h"

class USphereComponent;

/**
 * A place the bubbles can be called to. Its sphere, scaled as needed, is the
 * volume their cloud fills on arrival.
 */
UCLASS(Blueprintable, BlueprintType,
	HideCategories = (Actor, Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance),
	PrioritizeCategories = ("VibH2O"), AutoExpandCategories = ("VibH2O|Pilotage", "VibH2O|Fondu"),
	meta = (DisplayName = "VibH2O — Attracteur"))
class VIBH2O_API AVibH2OAttractorActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OAttractorActor();

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// =====================================================  Control

	/**
	 * Name used by the iPad and in OSC: /Attractor/<name>/ 1. Filled in from
	 * the actor's label when left empty. No spaces, no slashes: both are
	 * turned into '_' when typed in the Details panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Nom de contrôle",
			ToolTip = "Nom utilisé par l'iPad et l'OSC : /Attractor/<nom>/ 1. Vide : repris du nom de l'acteur. Les espaces et les « / » deviennent « _ »."))
	FName ControlName = NAME_None;

	/** Text on the iPad button. The control name when left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Nom affiché", ToolTip = "Texte du bouton sur l'iPad. Vide : le nom de contrôle."))
	FString DisplayLabel;

	/**
	 * Button colour on the iPad - tell attractors apart in the dark. Linear,
	 * like every colour in the Details panel: the iPad shows it in sRGB.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Couleur sur l'iPad",
			ToolTip = "Couleur du bouton sur l'iPad, pour distinguer les attracteurs dans le noir. Teinte aussi la sphère dans le viewport."))
	FLinearColor DisplayColor = FLinearColor(1.0f, 0.25f, 0.02f, 1.0f);

	/** Buttons are sorted by this, then by name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Ordre d'affichage", ToolTip = "Les boutons de l'iPad sont rangés selon ce numéro, puis par nom."))
	int32 DisplayOrder = 0;

	/**
	 * Target pull, 0 to 1. The iPad sends 0 or 1; anything between parks the
	 * bubbles part of the way. It is a target, not the pull itself: each
	 * bubble eases toward it over the fade times below, so flipping it is
	 * always smooth. Saved with the map: an attractor saved active starts
	 * active.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Pilotage",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Activation",
			ToolTip = "Cible d'appel, 0 à 1. Attention : sauvegarder la carte avec l'attracteur actif le rend actif au lancement."))
	float Activation = 0.0f;

	/** Follow /Attractor messages, from the iPad or from Max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Suivre l'OSC et l'iPad",
			ToolTip = "Décoché : l'attracteur ignore l'iPad et les messages /Attractor. Seuls Sequencer, les cues et les Blueprints le pilotent alors."))
	bool bFollowOsc = true;

	// =====================================================  Selection

	/**
	 * Share of the room this attractor calls, 0 to 1. The chosen bubbles are
	 * always the same ones, drawn from their seat, so a cue looks the same
	 * every night.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Sélection",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Part des bulles",
			ToolTip = "Part de la salle appelée, 0 à 1. Ce sont toujours les mêmes bulles, tirées d'après leur siège : une cue est identique chaque soir."))
	float Share = 0.3f;

	/**
	 * Where this attractor's share begins on the selection circle, 0 to 1.
	 *
	 * Every attractor reads the same circle. Two attractors at start 0 and
	 * share 0.3 call the same bubbles; start 0 and start 0.3 call two disjoint
	 * groups - which is how a room is split between two cues.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Sélection",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Début de la part",
			ToolTip = "Où commence la part sur le cercle de sélection, 0 à 1. Même début : les mêmes bulles. Début 0 et début 0,3 avec une part de 0,3 : deux groupes distincts."))
	float ShareStart = 0.0f;

	// =====================================================  Fades

	/** Time for the called bubbles to arrive, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fondu",
		meta = (ClampMin = "0.0", UIMax = "30.0", ForceUnits = "s", DisplayName = "Durée d'arrivée",
			ToolTip = "Temps d'arrivée des bulles appelées. 0 = saut immédiat."))
	float FadeInSeconds = 4.0f;

	/** Time for them to return to their place once released, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fondu",
		meta = (ClampMin = "0.0", UIMax = "30.0", ForceUnits = "s", DisplayName = "Durée de retour",
			ToolTip = "Temps de retour des bulles à leur place une fois relâchées. 0 = saut immédiat."))
	float FadeOutSeconds = 6.0f;

	/**
	 * Spread of the arrival times, 0 to 1. At 0 the group moves as one block;
	 * higher, the first leave well before the last, and the room flows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fondu",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Étalement",
			ToolTip = "0 : le groupe se déplace d'un bloc. Plus haut : les premières bulles partent bien avant les dernières, et la salle s'écoule."))
	float Stagger = 0.6f;

	// =====================================================  Cloud

	/** How much the gathered cloud swells and settles, 0 to 0.9. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Nuage",
		meta = (ClampMin = "0.0", ClampMax = "0.9", DisplayName = "Respiration",
			ToolTip = "Ampleur avec laquelle le nuage rassemblé gonfle et se resserre, 0 à 0,9."))
	float CloudBreathAmount = 0.12f;

	/** Breathing speed, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Nuage",
		meta = (ClampMin = "0.0", UIMax = "0.5", DisplayName = "Vitesse de respiration",
			ToolTip = "Vitesse de respiration du nuage, en cycles par seconde."))
	float CloudBreathSpeed = 0.07f;

	// =====================================================  Blueprint

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Pilotage")
	void SetActive(bool bActive);

	/** SetActive(true), as a button in the Details panel. */
	UFUNCTION(CallInEditor, Category = "VibH2O", meta = (DisplayName = "Appeler"))
	void CallBubbles();

	/** SetActive(false), as a button in the Details panel. */
	UFUNCTION(CallInEditor, Category = "VibH2O", meta = (DisplayName = "Renvoyer"))
	void ReleaseBubbles();

	/** Flips the target. Also a button in the Details panel, to try a cue in the editor. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Basculer"))
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	bool IsActive() const { return Activation > 0.5f; }

	/**
	 * Mean pull over the bubbles it calls, 0 to 1 - how far the cue has got.
	 * The ring round the iPad button shows this.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	float GetCurrentWeight() const { return CurrentWeight; }

	/** The control name, or the object name when none was given. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	FName GetEffectiveControlName() const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	FString GetEffectiveLabel() const;

	// =====================================================  Stage side
	//
	// Called by the stage once per frame, in this order: Begin, then for each
	// bubble StepBubbleWeight and GetCloudTarget, then End.

	void BeginSteeringFrame(const FTransform& StageRootTransform, float FadeScale, float DeltaSeconds, float TimeSeconds);

	/** Advances and returns this bubble's pull. Stepped once per frame at most. */
	float StepBubbleWeight(int32 SeatIndex, int32 Seed);

	/** Where this bubble sits in the cloud, in the stage's local space. */
	FVector GetCloudTarget(int32 Seed) const;

	void EndSteeringFrame();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O")
	TObjectPtr<USphereComponent> Volume;

private:
#if WITH_EDITOR
	void RefreshAutoControlName();
#endif

	/** The name last given from the label: a ControlName still equal to it follows the label. */
	UPROPERTY()
	FName LastAutoControlName = NAME_None;

	struct FBubbleWeight
	{
		FVibH2OSpring Spring;
		uint64 LastFrame = 0;
	};

	TMap<int32, FBubbleWeight> BubbleWeights;

	/** Attractor volume -> stage root, cached for the frame. */
	FTransform VolumeToStage = FTransform::Identity;

	float FrameFadeScale = 1.0f;
	float FrameDeltaSeconds = 0.0f;
	float FrameTimeSeconds = 0.0f;
	uint64 FrameNumber = 0;

	float FrameWeightSum = 0.0f;
	int32 FrameMemberCount = 0;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Avancement", ToolTip = "Où en est la cue, 0 à 1 : l'attraction moyenne sur les bulles appelées."))
	float CurrentWeight = 0.0f;
};
