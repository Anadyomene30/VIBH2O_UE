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
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "VibH2O — Attracteur"))
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
	 * the actor's label when left empty. No spaces, no slashes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Nom de controle"))
	FName ControlName = NAME_None;

	/** Text on the iPad button. The control name when left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Nom affiche"))
	FString DisplayLabel;

	/**
	 * Button colour on the iPad - tell attractors apart in the dark. Linear,
	 * like every colour in the Details panel: the iPad shows it in sRGB.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Couleur du bouton"))
	FLinearColor DisplayColor = FLinearColor(1.0f, 0.25f, 0.02f, 1.0f);

	/** Buttons are sorted by this, then by name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Ordre d'affichage"))
	int32 DisplayOrder = 0;

	/**
	 * Target pull, 0 to 1. The iPad sends 0 or 1; anything between parks the
	 * bubbles part of the way. It is a target, not the pull itself: each
	 * bubble eases toward it over the fade times below, so flipping it is
	 * always smooth.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Pilotage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Activation"))
	float Activation = 0.0f;

	/** Follow /Attractor messages, from the iPad or from Max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Suivre l'OSC et l'iPad"))
	bool bFollowOsc = true;

	// =====================================================  Selection

	/**
	 * Share of the room this attractor calls, 0 to 1. The chosen bubbles are
	 * always the same ones, drawn from their seat, so a cue looks the same
	 * every night.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Selection", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Part des bulles"))
	float Share = 0.3f;

	/**
	 * Where this attractor's share begins on the selection circle, 0 to 1.
	 *
	 * Every attractor reads the same circle. Two attractors at start 0 and
	 * share 0.3 call the same bubbles; start 0 and start 0.3 call two disjoint
	 * groups - which is how a room is split between two cues.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Selection", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Debut de la part"))
	float ShareStart = 0.0f;

	// =====================================================  Fades

	/** Time for the called bubbles to arrive, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fondu", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Duree d'arrivee"))
	float FadeInSeconds = 4.0f;

	/** Time for them to return to their place once released, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fondu", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Duree de retour"))
	float FadeOutSeconds = 6.0f;

	/**
	 * Spread of the arrival times, 0 to 1. At 0 the group moves as one block;
	 * higher, the first leave well before the last, and the room flows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fondu", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Etalement"))
	float Stagger = 0.6f;

	// =====================================================  Cloud

	/** How much the gathered cloud swells and settles, 0 to 0.9. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Nuage", meta = (ClampMin = "0.0", ClampMax = "0.9", DisplayName = "Respiration"))
	float CloudBreathAmount = 0.12f;

	/** Breathing speed, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Nuage", meta = (ClampMin = "0.0", DisplayName = "Vitesse de respiration"))
	float CloudBreathSpeed = 0.07f;

	// =====================================================  Blueprint

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Pilotage")
	void SetActive(bool bActive);

	/** Flips the target. Also a button in the Details panel, to try a cue in the editor. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Pilotage", meta = (DisplayName = "Basculer"))
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

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "VibH2O|Pilotage", meta = (DisplayName = "Avancement"))
	float CurrentWeight = 0.0f;
};
