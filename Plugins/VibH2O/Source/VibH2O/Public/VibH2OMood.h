// The mood of the room: calm, normal, excited.
//
// Max sends each person's arousal as a deviation from their own baseline. The
// mean of those deviations, empty seats and quiet sensors excluded, says how
// the room as a whole stands against its rest - and that is what the three
// moods read. Synchrony is a separate axis: a room can be in step while calm
// as well as while excited, so nothing here looks at it.
//
// The mood is discrete, with hysteresis, so it cannot flicker at a threshold.
// Its RENDERING is continuous: three weights that cross-fade towards the new
// mood over a few seconds. Everything the bubbles show is a blend of three
// looks by those weights.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "VibH2OMood.generated.h"

/** The three moods of the room. */
UENUM(BlueprintType)
enum class EVibH2OMood : uint8
{
	Calm UMETA(DisplayName = "Calme"),
	Normal UMETA(DisplayName = "Normal"),
	Excited UMETA(DisplayName = "Excite")
};

/** Who decides the mood: the room, or the operator. */
UENUM(BlueprintType)
enum class EVibH2OMoodOverride : uint8
{
	/** The room's mean arousal decides. */
	Auto UMETA(DisplayName = "Automatique (la salle decide)"),
	Calm UMETA(DisplayName = "Force : calme"),
	Normal UMETA(DisplayName = "Force : normal"),
	Excited UMETA(DisplayName = "Force : excite")
};

/** How much of each mood is showing. Always sums to one. */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OMoodWeights
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambiance")
	float Calm = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambiance")
	float Normal = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambiance")
	float Excited = 0.0f;

	static FVibH2OMoodWeights For(EVibH2OMood Mood);
	static FVibH2OMoodWeights Lerp(const FVibH2OMoodWeights& A, const FVibH2OMoodWeights& B, float Alpha);
};

/**
 * What one mood looks like on the bubbles.
 *
 * Colour, energy, motion and size: the four things the piece asked to change
 * from one mood to the next. Every value here is a target the stage blends
 * between moods, never applied raw - so a value can be pushed far without the
 * change ever arriving as a jump.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OMoodLook
{
	GENERATED_BODY()

	/**
	 * Colour of a bubble calmer than the room. Each bubble sits between this
	 * colour and the next one according to how it stands against the room's
	 * mean - so a mood's palette is always used in full, whatever the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Couleur", meta = (DisplayName = "Couleur : plus calme que la salle", HideAlphaChannel))
	FLinearColor ColorLow = FLinearColor(0.02f, 0.30f, 0.38f, 1.0f);

	/** Colour of a bubble more excited than the room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Couleur", meta = (DisplayName = "Couleur : plus excitee que la salle", HideAlphaChannel))
	FLinearColor ColorHigh = FLinearColor(0.10f, 0.62f, 0.55f, 1.0f);

	/** Light carried by the striations, as a multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Energie", meta = (DisplayName = "Lumiere des stries", ClampMin = "0.0", UIMax = "4.0"))
	float Glow = 1.0f;

	/** How fast the striations run, in bands per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Energie", meta = (DisplayName = "Vitesse des stries", ClampMin = "0.0", UIMax = "3.0"))
	float StripeSpeed = 0.3f;

	/** Restlessness of the surface: striation warp and silhouette breathing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Energie", meta = (DisplayName = "Agitation de surface", ClampMin = "0.0", UIMax = "3.0"))
	float Turbulence = 1.0f;

	/** Multiplies the amplitude of the permanent float. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Mouvement", meta = (DisplayName = "Ampleur du flottement", ClampMin = "0.0", UIMax = "3.0"))
	float FloatScale = 1.0f;

	/** Multiplies the speed of the permanent float. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Mouvement", meta = (DisplayName = "Vitesse du flottement", ClampMin = "0.0", UIMax = "3.0"))
	float FloatSpeedScale = 1.0f;

	/** Multiplies the arousal-driven drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Mouvement", meta = (DisplayName = "Derive", ClampMin = "0.0", UIMax = "3.0"))
	float DriftScale = 1.0f;

	/** Multiplies the size of every bubble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Taille", meta = (DisplayName = "Taille", ClampMin = "0.1", UIMax = "2.0"))
	float SizeScale = 1.0f;

	/** Multiplies the swell on each heartbeat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Taille", meta = (DisplayName = "Force du battement", ClampMin = "0.0", UIMax = "3.0"))
	float PulseScale = 1.0f;

	/** Weighted blend of the three moods' looks. */
	static FVibH2OMoodLook Blend(const FVibH2OMoodLook& Calm, const FVibH2OMoodLook& Normal, const FVibH2OMoodLook& Excited,
		const FVibH2OMoodWeights& Weights);

	/** Colour for a bubble at a position from 0 (calmer than the room) to 1. */
	FLinearColor ColorAt(float Position) const;
};

/**
 * The three looks, plus how individuals and synchrony play inside them.
 *
 * Defaults run cold to warm: deep blues when calm, the aqua of the water in
 * between, amber to coral when excited.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OMoodPalette
{
	GENERATED_BODY()

	FVibH2OMoodPalette();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Etats", meta = (DisplayName = "Calme"))
	FVibH2OMoodLook Calm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Etats", meta = (DisplayName = "Normal"))
	FVibH2OMoodLook Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Etats", meta = (DisplayName = "Excite"))
	FVibH2OMoodLook Excited;

	/**
	 * How far a person's own arousal moves their bubble across the palette.
	 * The room's mean sits in the middle; a gap of 1 / contrast reaches either
	 * end. Zero: every bubble takes the palette's middle colour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Individus", meta = (DisplayName = "Contraste individuel", ClampMin = "0.0", UIMax = "6.0"))
	float IndividualContrast = 2.5f;

	/**
	 * Synchrony draws the hues together: at full synchrony this share of each
	 * bubble's own nuance is given up for the room's. Out of step, every
	 * bubble keeps its own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Synchronie", meta = (DisplayName = "Synchronie : teintes resserrees", ClampMin = "0.0", ClampMax = "1.0"))
	float SyncHueTightening = 0.75f;

	/** Strength of the shared halo synchrony lights on the rims. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Synchronie", meta = (DisplayName = "Synchronie : halo", ClampMin = "0.0", UIMax = "3.0"))
	float SyncHalo = 1.0f;

	const FVibH2OMoodLook& Get(EVibH2OMood Mood) const;
	FVibH2OMoodLook Blend(const FVibH2OMoodWeights& Weights) const;
};

/**
 * A mood preset: palettes and energy for the three moods, in an asset.
 *
 * The stage plays its own copy, so Sequencer can animate it and a tweak in
 * the Details panel shows at once. The preset is where that copy comes from
 * and where it goes back to: drop one on the stage to load it, press "Save to
 * preset" to keep a tuning. Swapping between scenes is one drag.
 */
UCLASS(BlueprintType, meta = (DisplayName = "VibH2O - Preset d'ambiance"))
class VIBH2O_API UVibH2OMoodPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance", meta = (ShowOnlyInnerProperties))
	FVibH2OMoodPalette Palette;
};

/** Settings the tracker reads every frame. Plain data: no engine needed. */
struct VIBH2O_API FVibH2OMoodTrackerSettings
{
	/** Below this collective arousal (with hysteresis), the room is calm. */
	float CalmThreshold = 0.25f;

	/** Above this collective arousal (with hysteresis), the room is excited. */
	float ExcitedThreshold = 0.55f;

	/** Half-width of the dead band around each threshold. */
	float Hysteresis = 0.04f;

	/** Time constant smoothing the collective arousal before it is judged. */
	float DetectionSmoothingSeconds = 1.5f;

	/** Length of the cross-fade from one mood to the next. */
	float TransitionSeconds = 4.0f;

	EVibH2OMoodOverride Override = EVibH2OMoodOverride::Auto;
};

/**
 * Turns the room's mean arousal into a mood and its cross-fade weights.
 *
 * Kept free of any UObject so the threshold logic - where a flickering bug
 * would live - is testable on bare numbers.
 */
class VIBH2O_API FVibH2OMoodTracker
{
public:
	/**
	 * Advances by one frame. bHasData false holds the measurement where it is:
	 * a room with no live sensor has no mood to report, and must not slide to
	 * "calm" merely because its mean reads zero. Transitions already under way
	 * still finish, and an override still applies.
	 *
	 * @return true when the mood changed this frame.
	 */
	bool Update(float CollectiveExcitation, bool bHasData, float DeltaSeconds, const FVibH2OMoodTrackerSettings& Settings);

	/** Jumps straight to the mood the given arousal implies, no cross-fade. */
	void Snap(float CollectiveExcitation, const FVibH2OMoodTrackerSettings& Settings);

	/**
	 * The mood an arousal value implies, starting from the current one.
	 *
	 * Leaving a mood requires crossing its threshold by the hysteresis margin,
	 * so a value hovering on a threshold keeps whichever mood it already had.
	 */
	static EVibH2OMood Classify(float Excitation, EVibH2OMood Current, const FVibH2OMoodTrackerSettings& Settings);

	EVibH2OMood GetMood() const { return Mood; }
	const FVibH2OMoodWeights& GetWeights() const { return Weights; }
	float GetSmoothedExcitation() const { return Smoothed; }
	bool IsTransitioning() const { return Progress < 1.0f; }

private:
	void StartTransition(EVibH2OMood NewMood, float TransitionSeconds);

	float Smoothed = 0.0f;
	bool bInitialised = false;
	EVibH2OMood Mood = EVibH2OMood::Normal;
	FVibH2OMoodWeights From;
	FVibH2OMoodWeights Weights;
	float Progress = 1.0f;
};
