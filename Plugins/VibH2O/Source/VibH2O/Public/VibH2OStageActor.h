// The stage actor: the root of the installation.
//
// It owns the bubbles - permanently, whichever tableau is showing. Every
// position is computed in LOCAL space under its root, which makes the
// installation movable, rotatable and scalable in the level without a single
// line of code noticing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Layout/VibH2OLayoutMath.h"
#include "Layout/VibH2OPositionDriver.h"
#include "VibH2OMood.h"
#include "VibH2OTypes.h"

#include "VibH2OStageActor.generated.h"

class AVibH2OAttractorActor;
class AVibH2OBubbleActor;
class AVibH2OExclusionZoneActor;
class UCurveFloat;
class UVibH2OMoodPreset;
class UVibH2OSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OStageFocusChangedSignature, FName, GroupName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OStageRoomBuiltSignature, int32, BubbleCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVibH2OStageMoodChangedSignature, EVibH2OMood, NewMood, EVibH2OMood, PreviousMood);

/** The daughters born from one seat's bubble. A struct only because a TMap cannot hold a TArray as a UPROPERTY. */
USTRUCT()
struct FVibH2OBubbleFamily
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVibH2OBubbleActor>> Daughters;
};

/**
 * The room, and the tableaux that move it about.
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "VibH2O — Acteur de scene"))
class VIBH2O_API AVibH2OStageActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OStageActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// =====================================================  Mood (quick settings)
	//
	// What the get-in needs, and nothing else, first in the Details panel. The
	// rest of the category is under the advanced arrow.

	/**
	 * Palettes and energy for the three moods. Dropping a preset here loads it
	 * into the stage's own copy below; "Save to preset" writes a tuning back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VibH2O|Ambiance", meta = (DisplayName = "Preset d'ambiance"))
	TObjectPtr<UVibH2OMoodPreset> MoodPreset;

	/**
	 * Below this mean arousal the room turns calm. Read against "Room arousal"
	 * just below, which shows the live value to set it from.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Seuil calme", ClampMin = "0.0", ClampMax = "1.0"))
	float CalmThreshold = 0.25f;

	/** Above this mean arousal the room turns excited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Seuil excite", ClampMin = "0.0", ClampMax = "1.0"))
	float ExcitedThreshold = 0.55f;

	/** Lets the operator force a mood - rehearsal, or a cue in the show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Forcer l'etat"))
	EVibH2OMoodOverride MoodOverride = EVibH2OMoodOverride::Auto;

	/** The mood the room is in, or fading towards. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "VibH2O|Ambiance", meta = (DisplayName = "Etat actuel"))
	EVibH2OMood CurrentMood = EVibH2OMood::Normal;

	/**
	 * Mean arousal of the room, smoothed as the thresholds see it: the number
	 * to watch while setting them, since the range Max sends is not known in
	 * advance.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "VibH2O|Ambiance", meta = (DisplayName = "Excitation de la salle"))
	float RoomExcitation = 0.0f;

	/** Overall light of the bubbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Lumiere des bulles", ClampMin = "0.0", UIMax = "4.0"))
	float BubbleBrightness = 1.0f;

	/** Base scale of a bubble at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Taille des bulles", ClampMin = "0.01", UIMax = "4.0"))
	float BaseScale = 1.0f;

	/**
	 * The palettes and energy the stage actually plays: colour, striations,
	 * motion and size for each mood. Animatable in Sequencer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Palettes par etat"))
	FVibH2OMoodPalette MoodPalette;

	/** How long the room takes to cross over from one mood to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Duree de bascule", ClampMin = "0.0", UIMax = "20.0", ForceUnits = "s"))
	float MoodTransitionSeconds = 4.0f;

	/**
	 * Dead band around each threshold. Leaving a mood takes crossing its
	 * threshold by this much, so a room hovering at the border cannot flicker.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Marge anti-clignotement", ClampMin = "0.0", ClampMax = "0.25"))
	float MoodHysteresis = 0.04f;

	/** Time constant smoothing the room's arousal before it is judged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Lissage de la mesure", ClampMin = "0.0", UIMax = "10.0", ForceUnits = "s"))
	float MoodDetectionSmoothing = 1.5f;

	/**
	 * How much a bubble's own synchrony counts against the room's, when it
	 * falls in step. Zero: the room's synchrony alone. One: each person's own.
	 * In between, a person out of step stands out in a synchronous room.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Part de la synchronie individuelle", ClampMin = "0.0", ClampMax = "1.0"))
	float IndividualCoherenceShare = 0.5f;

	/** Loads a preset into the palettes the stage plays. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Ambiance")
	void ApplyMoodPreset(UVibH2OMoodPreset* Preset);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Ambiance")
	void SetMoodOverride(EVibH2OMoodOverride NewOverride);

	/** Jumps to the mood the room implies right now, without cross-fade. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Ambiance")
	void SnapMood();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Ambiance")
	EVibH2OMood GetCurrentMood() const { return CurrentMood; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Ambiance")
	FVibH2OMoodWeights GetMoodWeights() const { return MoodTracker.GetWeights(); }

	/** Writes the palettes the stage plays back into the preset asset. */
	UFUNCTION(CallInEditor, Category = "VibH2O|Ambiance", meta = (DisplayName = "Enregistrer dans le preset"))
	void SaveMoodPreset();

	/** Reloads the preset, dropping any tuning not saved into it. */
	UFUNCTION(CallInEditor, Category = "VibH2O|Ambiance", meta = (DisplayName = "Recharger le preset"))
	void ReloadMoodPreset();

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OStageMoodChangedSignature OnMoodChanged;

	// =====================================================  Bubbles

	/** Class instanced for each occupied seat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Bulles")
	TSubclassOf<AVibH2OBubbleActor> BubbleClass;

	/** Extra growth at full arousal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0"))
	float ScalePerExcitation = 0.35f;

	/**
	 * Amplitude of the "pop" on each beat. Kept small: a living thing swells
	 * with its pulse, it does not bounce like a cartoon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0"))
	float BeatPopAmount = 0.10f;

	/**
	 * How much each bubble deforms out of a perfect sphere, 0 to 1.
	 *
	 * A room of identical spheres reads as manufactured no matter how good the
	 * material is. Each bubble breathes on three axes at slightly different
	 * rates, with a phase of its own, so no two are the same shape at the same
	 * instant - and arousal deepens it.
	 *
	 * Realism lives in the low values. A few percent reads as a soft body held
	 * by water; past ten the bubble visibly stretches like rubber, which is
	 * what the earlier default of 0.16 did.
	 *
	 * This lives here rather than in the material because Unreal 5.5 does not
	 * expose the world position offset input to Python, so a generated material
	 * cannot reach it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float OrganicDeform = 0.035f;

	/** How fast that deformation breathes, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0"))
	float OrganicSpeed = 0.42f;

	/** Converts the measured speed (cm/s) into striation speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0"))
	float StriationSpeedScale = 0.01f;

	// =====================================================  Geometry

	/** Lateral gap between two neighbouring seats, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float ColumnSpacing = 120.0f;

	/** Depth gap between two rows, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float RowSpacing = 140.0f;

	/**
	 * SIGNED angle per column step, in degrees. Zero gives an exactly straight
	 * room; positive and negative curve it either way.
	 *
	 * The angle is set per seat rather than as a radius because a straight room
	 * would correspond to an infinite radius, impossible to type into a field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "-30.0", ClampMax = "30.0"))
	float CurvatureAngle = 0.0f;

	/**
	 * True: constant angle per seat, so the back rows are wider - the geometry
	 * of a real theatre. False: constant lateral gap everywhere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie")
	bool bFanOut = true;

	/**
	 * Bubbles rotate to face the centre of curvature.
	 *
	 * This is not decoration: the bubbles carry directional striations. Were
	 * they all to keep the same heading on a curved room, the striations would
	 * all point the same way and give the curvature away at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie")
	bool bOrientToCenter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie")
	FRotator OrientationOffset = FRotator::ZeroRotator;

	/** Vertical rise per row, to reproduce raked seating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ForceUnits = "cm"))
	float ElevationPerRow = 0.0f;

	/** Optional curve for a non-linear rake, evaluated over [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Geometrie")
	TObjectPtr<UCurveFloat> ElevationCurve;

	/** Amplitude of the depth offset, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DepthAmplitude = 0.0f;

	/**
	 * Scale of the noise producing that offset. The relief comes from noise and
	 * not from per-seat randomness: pure randomness would decorrelate
	 * neighbours and destroy the reading of the rows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Geometrie", meta = (ClampMin = "0.01"))
	float DepthNoiseScale = 0.25f;

	// =====================================================  Movement

	/** Amplitude of the permanent float, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FloatAmplitude = 12.0f;

	/** Float speed, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0"))
	float FloatSpeed = 0.35f;

	/** Amplitude of the slow pitch that accompanies the float, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0"))
	float FloatRotationAmount = 6.0f;

	/** Below this arousal threshold, complete stillness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float DriftThreshold = 0.4f;

	/** Drift amplitude at full arousal, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DriftIntensity = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0"))
	float DriftDirectionSpeed = 0.12f;

	/**
	 * Fraction of the seat spacing the drift may not exceed. The maximum is
	 * deliberately capped at 0.49: below half a cell, a bubble mathematically
	 * cannot reach its neighbour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float DriftMaxRatio = 0.35f;

	// =====================================================  Tableaux

	/**
	 * Morph from tableau 1 to tableau 2, 0 to 1.
	 *
	 * Both drivers run permanently; this slider only interpolates between them.
	 * The transition is therefore continuous, reversible and stoppable half
	 * way, with no state machine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendAlpha = 0.0f;

	/** Easing curve for the morph. Null means linear interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tableaux")
	TObjectPtr<UCurveFloat> BlendCurve;

	/** Tableau 1 driver. Replaceable by a Blueprint subclass. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "VibH2O|Tableaux")
	TObjectPtr<UVibH2OPositionDriver> GridDriver;

	/**
	 * Tableau 2 driver. Defaults to the boids school; swap in the vortex driver
	 * for the simpler cone.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "VibH2O|Tableaux")
	TObjectPtr<UVibH2OPositionDriver> FlockDriver;

	// -------- Vortex settings, read by the tableau 2 driver.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float VortexHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float VortexBaseRadius = 200.0f;

	/** Larger than the base radius for a cone that opens upward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float VortexTopRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex")
	float VortexTurns = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex")
	float VortexSpinSpeed = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0"))
	float VortexAngleDispersion = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0"))
	float VortexRadiusDispersion = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0"))
	float VortexHeightDispersion = 0.12f;

	/**
	 * Share of individual synchrony in the scatter. Zero lets collective
	 * synchrony alone set the order, as the piece intends.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VortexIndividualInfluence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ForceUnits = "cm"))
	float VortexZOffset = 0.0f;

	// -------- Division, on the way to tableau 2.

	/**
	 * Daughters born from each bubble on the way to tableau 2.
	 *
	 * A room of a hundred and fifty makes a thin school. Each bubble divides
	 * into sisters of equal size - its volume shared, its heart and colour
	 * kept - so the shoal gains a body while every family still belongs to one
	 * spectator. Zero turns the division off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Filles par bulle", ClampMin = "0", ClampMax = "8"))
	int32 DaughtersPerBubble = 3;

	/**
	 * Ceiling on the whole school, mothers included. Past it, fewer daughters
	 * are born rather than the frame rate falling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Plafond du banc", ClampMin = "1"))
	int32 MaxSchoolSize = 1000;

	/** BlendAlpha at which the bubbles begin to divide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Division : debut", ClampMin = "0.0", ClampMax = "1.0"))
	float DivisionStart = 0.0f;

	/**
	 * BlendAlpha at which the division is complete. The families leave their
	 * seats half way through it, so the two gestures overlap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Division : fin", ClampMin = "0.0", ClampMax = "1.0"))
	float DivisionEnd = 0.35f;

	/** How far the sisters stand from their seat once divided, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Ecart des soeurs", ClampMin = "0.0", ForceUnits = "cm"))
	float DivisionSpread = 35.0f;

	// -------- Order of the school, read by the tableau 2 boids driver.

	/** Below this collective synchrony the school is fully dispersed into families. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Synchronie : dispersion sous", ClampMin = "0.0", ClampMax = "1.0"))
	float SchoolDisperseBelow = 0.35f;

	/** Above this collective synchrony the whole school mills as one vortex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Synchronie : vortex au-dessus de", ClampMin = "0.0", ClampMax = "1.0"))
	float SchoolVortexAbove = 0.70f;

	/**
	 * Seconds the school takes to follow a change of synchrony. Synchrony is a
	 * noisy measurement; without this the vortex would tremble.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Lissage", ClampMin = "0.0", ForceUnits = "s"))
	float SchoolOrderSmoothing = 2.0f;

	/**
	 * 1: the room's synchrony decides the order of the school. 0: "Ordre
	 * manuel" decides. In between, a mix - to hand control to the director
	 * for a moment, or to rehearse without an audience.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Part de la synchronie", ClampMin = "0.0", ClampMax = "1.0"))
	float SchoolSyncInfluence = 1.0f;

	/** Order used instead of the synchrony's: 0 dispersed into families, 1 vortex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Ordre manuel", ClampMin = "0.0", ClampMax = "1.0"))
	float SchoolManualOrder = 0.0f;

	/** Members per family on stage, mother included: 1 + the daughters actually born. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tableaux")
	int32 GetFamilySize() const { return FamilySize; }

	/** Order of the school in force, 0 dispersed to 1 vortex. 0 when tableau 2 is not the boids driver. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tableaux")
	float GetSchoolOrder() const;

	/** Division progress in force, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tableaux")
	float GetDivisionProgress() const;

	/** Every daughter bubble, all families together. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tableaux")
	TArray<AVibH2OBubbleActor*> GetDaughters() const;

	/** The daughters of one seat's bubble. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tableaux")
	TArray<AVibH2OBubbleActor*> GetDaughtersOfSeat(int32 InSeatIndex) const;

	// =====================================================  Focus

	/** Named groups, defined here in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus")
	TArray<FVibH2OFocusGroup> FocusGroups;

	/** Currently selected group. Empty means no focus at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VibH2O|Focus")
	FName ActiveFocusGroup = NAME_None;

	/**
	 * Focus intensity, 0 to 1. Nothing moves: the room plan stays readable,
	 * only the bubbles outside the group are dimmed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Focus", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FocusAmount = 0.0f;

	/** Mask value for a bubble outside the group, at full intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Focus", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FocusDimmedValue = 0.12f;

	/** Margin applied to the computed pull-back distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Focus", meta = (ClampMin = "1.0"))
	float FocusFitMargin = 1.15f;

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Focus")
	void SetActiveFocusGroup(FName GroupName);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Focus")
	void SelectFocusGroupByIndex(int32 Index);

	/** Steps to the next or previous group. Handy to wire onto a key. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Focus")
	void CycleFocusGroup(int32 Delta = 1);

	/**
	 * Bounds of the focused group, in WORLD space.
	 *
	 * Computed from the positions the bubbles actually occupy, never from an
	 * assumed flat grid - otherwise the framing would aim beside the target the
	 * moment the room is curved or the actor moved.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Focus")
	FBox GetFocusBounds() const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Focus")
	FVector GetFocusCenter() const;

	/**
	 * Bounds of every bubble, in WORLD space, focus or not.
	 *
	 * Taken from where the bubbles actually are this frame, so it follows a
	 * resize, a curvature, a moved stage and the morph into the school alike -
	 * which is what lets a light rig keep the whole room framed. Falls back to
	 * a small box round the stage when there is no bubble.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O")
	FBox GetRoomBounds() const;

	/**
	 * Distance at which a camera of the given horizontal field of view frames
	 * the whole group.
	 *
	 * @param HorizontalFOV Horizontal field of view, in degrees.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Focus")
	float GetFocusFitDistance(float HorizontalFOV) const;

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OStageFocusChangedSignature OnFocusGroupChanged;

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OStageRoomBuiltSignature OnRoomBuilt;

	// =====================================================  OSC control

	/**
	 * Follow /Blend as received over OSC.
	 *
	 * Arbitration with Sequencer takes care of itself: OSC only writes when a
	 * message arrives, whereas Sequencer rewrites the property every frame
	 * during playback. Sequencer therefore wins on its own, and the subsystem's
	 * bIgnoreOscControl cuts the input outright during a render.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Controle")
	bool bFollowOscBlend = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Controle")
	bool bFollowOscFocus = true;

	/** Follow /Attractors/FadeScale and /Attractors/Release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Controle")
	bool bFollowOscSteering = true;

	// =====================================================  Attractors and zones
	//
	// Hand-steered layers on top of the tableaux, placed in the level as
	// AVibH2OAttractorActor and AVibH2OExclusionZoneActor. The stack becomes:
	//
	//   Lerp(Grid, Flock, Blend)  ->  attractors  ->  float + drift  ->  exclusion
	//
	// The drivers keep running under the attractors, which is why releasing one
	// brings every bubble back to where it would have been: nothing had to
	// remember it.

	/**
	 * Multiplies every attractor's fade times: 2 makes every cue twice as
	 * slow. The iPad's speed slider drives this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Attracteurs", meta = (ClampMin = "0.1", ClampMax = "10.0", DisplayName = "Multiplicateur des fondus"))
	float AttractorFadeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Attracteurs", meta = (DisplayName = "Appliquer les attracteurs"))
	bool bApplyAttractors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Attracteurs", meta = (DisplayName = "Appliquer les zones d'exclusion"))
	bool bApplyExclusionZones = true;

	/** Every attractor that follows OSC lets go, each over its own fade. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Attracteurs")
	void ReleaseAllAttractors();

	/** The attractors in this stage's level, in iPad order. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Attracteurs")
	TArray<AVibH2OAttractorActor*> GetAttractors() const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Attracteurs")
	TArray<AVibH2OExclusionZoneActor*> GetExclusionZones() const;

	/**
	 * The stage builds what the iPad shows - attractors, zones, blend, focus,
	 * effects - and hands it to the remote a few times a second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Telecommande", meta = (DisplayName = "Publier l'etat pour l'iPad"))
	bool bPublishRemoteState = true;

	/** The remote's state as JSON. Public for the tests and for diagnosis. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Telecommande")
	FString BuildRemoteStateJson() const;

	// =====================================================  Editor preview

	/**
	 * Draws the room in the editor viewport without creating a single actor.
	 *
	 * This is what allows sweeping the curvature and checking the orientation
	 * without launching the game or the simulator. No bubble is instanced, so
	 * the level stays clean.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation")
	bool bPreviewInEditor = true;

	/**
	 * Real bubbles in the editor, no Play needed.
	 *
	 * As soon as something feeds the editor's stand-in subsystem - the
	 * simulator actor does, out of the box - the stage creates its real
	 * bubbles, with their material, their lighting and the full data path,
	 * right in the editor viewport. They are temporary: never saved with the
	 * level, absent from the outliner, and not selectable, so a click in the
	 * room cannot grab one by mistake. Select the stage from the outliner.
	 *
	 * The viewport must be in real time (Ctrl+R) for anything to move.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation", meta = (DisplayName = "Bulles vivantes dans l'editeur"))
	bool bLiveInEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation", meta = (ClampMin = "1", ClampMax = "64"))
	int32 PreviewColumns = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation", meta = (ClampMin = "1", ClampMax = "64"))
	int32 PreviewRows = 3;

	/** Also draws one arrow per seat, to check the orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation")
	bool bPreviewShowOrientation = true;

	/**
	 * Writes each individual's number over their bubble.
	 *
	 * How a seating plan is checked against the real room, and how a wrong seat
	 * order is caught in one look rather than on opening night.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Bulles")
	bool bShowSeatNumbers = false;

	/**
	 * Animate the preview: the float, and a fake heartbeat pulsing the markers.
	 *
	 * No data exists in the editor - there is no game world, so no subsystem
	 * and no individuals. The preview therefore invents a plausible BPM per
	 * seat, purely so the room reads as alive while the geometry is being
	 * dialled in. It proves nothing about real data; that is what Simulate and
	 * Play are for.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation")
	bool bPreviewAnimate = true;

	// =====================================================  Access

	UFUNCTION(BlueprintPure, Category = "VibH2O")
	int32 GetBubbleCount() const { return Bubbles.Num(); }

	UFUNCTION(BlueprintPure, Category = "VibH2O")
	AVibH2OBubbleActor* GetBubbleAtSeat(int32 InSeatIndex) const;

	UFUNCTION(BlueprintPure, Category = "VibH2O")
	AVibH2OBubbleActor* GetBubbleForIndividual(int32 InIndividualId) const;

	UFUNCTION(BlueprintPure, Category = "VibH2O")
	TArray<AVibH2OBubbleActor*> GetBubbles() const;

	/** Root of every local position. */
	UFUNCTION(BlueprintPure, Category = "VibH2O")
	USceneComponent* GetStageRoot() const { return StageRoot; }

	/** Seconds since this actor started ticking. Advances in the editor too. */
	UFUNCTION(BlueprintPure, Category = "VibH2O")
	float GetStageTime() const { return StageTime; }

	/** Forces a full rebuild. Rarely useful; it serves diagnosis. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O")
	void RebuildNow();

protected:
	UFUNCTION()
	void HandleRoomChanged(const FVibH2ORoomSnapshot& NewRoom);

	UFUNCTION()
	void HandleOscBlendAlpha(float Value);

	UFUNCTION()
	void HandleOscFocusAmount(float Value);

	UFUNCTION()
	void HandleOscFocusGroup(FName GroupName);

	UFUNCTION()
	void HandleOscAttractor(FName Name, float Value);

	UFUNCTION()
	void HandleOscAttractorShare(FName Name, float Value);

	UFUNCTION()
	void HandleOscAttractorsReleased();

	UFUNCTION()
	void HandleOscAttractorFadeScale(float Value);

	UFUNCTION()
	void HandleOscExclusion(FName Name, float Value);

	UFUNCTION()
	void HandleOscExclusionMoved(FName Name, FVector2D Position);

private:
	void BindToSubsystem(UVibH2OSubsystem* Subsystem);
	void UnbindFromSubsystem();
	void DestroyBubbles();

	/**
	 * Editor-world tick: advances the stand-in subsystem and moves the real
	 * bubbles. Returns false when there is nothing live to show, so the
	 * debug-drawn preview can take over.
	 */
	bool TickEditorLive(float DeltaSeconds);

	/** Bounds of the bubbles that pass the filter, padded by a bubble radius. */
	FBox ComputeBubbleBounds(const FVibH2OFocusGroup* Group) const;

	void ReconcileBubbles(const FVibH2ORoomSnapshot& NewRoom);

	/** Spawns one bubble under the stage root, with the editor-scaffolding flags when needed. */
	AVibH2OBubbleActor* SpawnBubbleActor(UWorld* World, const UClass* SpawnClass);

	/**
	 * Brings every family to the size the settings ask for: daughters are born
	 * with the room, never on the fly during the morph, so the division is a
	 * pure function of BlendAlpha and can run backwards.
	 */
	void ReconcileDaughters();
	void DestroyDaughters();

	/** Hides a daughter and folds her to a speck: invisible in game and in the editor viewport alike. */
	static void TuckAway(AVibH2OBubbleActor* Daughter);
	void TuckAwayDaughters();

	void UpdateBubbles(float DeltaSeconds);
	void DrawEditorPreview() const;

	FVibH2OGridParams MakeGridParams(int32 Columns, int32 Rows) const;
	const FVibH2OFocusGroup* FindFocusGroup(FName GroupName) const;
	float ComputeFocusMask(const FVibH2OSeat& Seat) const;
	/**
	 * Advances the mood from the room's mean arousal and blends the three
	 * looks into this frame's. Runs every tick, bubbles or not, so the panel
	 * and OnMoodChanged stay live.
	 */
	void UpdateMood(const UVibH2OSubsystem* Subsystem, float DeltaSeconds);

	FVibH2OMoodTrackerSettings MakeMoodSettings() const;

	UVibH2OSubsystem* GetSubsystem() const;

	/** Collects this frame's attractors and zones and steps their fades. */
	void BeginSteering(float DeltaSeconds);
	void EndSteering();

	/** The attractor layer, for one bubble, in local space. */
	FVector ApplyAttractors(const FVector& Base, int32 SeatIndex, int32 Seed);

	/** The exclusion layer, for one bubble, in local space. Always last. */
	FVector ApplyExclusionZones(const FVector& Location, int32 SeatIndex, int32 Seed);

	void PublishRemoteState(float DeltaSeconds);

	AVibH2OAttractorActor* FindAttractor(FName ControlName) const;
	AVibH2OExclusionZoneActor* FindExclusionZone(FName ControlName) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> StageRoot;

	/** Live bubbles, keyed by seat index. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AVibH2OBubbleActor>> Bubbles;

	/** Daughters, keyed by their mother's seat index. They belong to the stage, like the bubbles. */
	UPROPERTY(Transient)
	TMap<int32, FVibH2OBubbleFamily> Families;

	/** Members per family currently spawned, mother included. */
	int32 FamilySize = 1;

	/**
	 * True while every daughter is tucked away, at division progress zero.
	 * Tableau 1 then skips them entirely: five hundred hidden bubbles cost
	 * nothing instead of a full update each.
	 */
	bool bDaughtersTucked = false;

	/** Local copy of the plan, so we do not query the subsystem every frame. */
	UPROPERTY(Transient)
	FVibH2ORoomSnapshot Room;

	/**
	 * Seconds since this actor started ticking - the clock every float, drift
	 * and vortex spin reads from.
	 *
	 * Exposed to Blueprint because an artist driving their own effects needs
	 * the same clock the plugin uses, and because it is the plainest evidence
	 * that the actor ticks in the editor: outside Play it still climbs.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "VibH2O", meta = (AllowPrivateAccess = "true"))
	float StageTime = 0.0f;

	/**
	 * The subsystem whose delegates we are bound to. Kept rather than looked up
	 * again on unbind: in the editor it is the stand-in, which a world cleanup
	 * may already have replaced.
	 */
	TWeakObjectPtr<UVibH2OSubsystem> BoundSubsystem;

	/** Turns the room's arousal into a mood. Plain C++, tested on its own. */
	FVibH2OMoodTracker MoodTracker;

	/** This frame's look, blended across the three moods by their weights. */
	FVibH2OMoodLook BlendedLook;

	/**
	 * Clock of the permanent float. It advances at the mood's float speed
	 * rather than reading StageTime * speed: a change of mood then changes
	 * how fast the bubbles bob, never where they are.
	 */
	float FloatClock = 0.0f;

	/** Striation phase shared by the whole room, 0 to 1. */
	float RoomStripePhase = 0.0f;
	/** This frame's steering actors, gathered by BeginSteering. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AVibH2OAttractorActor>> FrameAttractors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVibH2OExclusionZoneActor>> FrameZones;

	float RemotePublishTimer = 0.0f;
};
