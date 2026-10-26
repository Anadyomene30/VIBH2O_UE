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
UCLASS(Blueprintable, BlueprintType,
	HideCategories = (Actor, Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance),
	PrioritizeCategories = ("VibH2O"),
	AutoExpandCategories = ("VibH2O|Ambiance", "VibH2O|Tableaux"),
	meta = (DisplayName = "VibH2O — Acteur de scène"))
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

	// =====================================================  Show day

	/**
	 * Puts every show control back where a performance starts: tableau 1,
	 * no focus, automatic mood, attractors released, zones on and centred,
	 * backdrop present. The values the editor buttons and the iPad leave
	 * behind are saved with the map otherwise.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Remettre en état de début de spectacle", ToolTip = "Tableau 1, pas de focus, ambiance automatique, attracteurs relâchés, zones allumées et recentrées, fond présent. À faire avant d'enregistrer la carte du spectacle."))
	void ResetForShowStart();

	/** Lists, in a dialog and the log, whatever would surprise a performance. */
	UFUNCTION(CallInEditor, Category = "VibH2O", meta = (DisplayName = "Vérifier avant la représentation", ToolTip = "Signale ce qui risque de surprendre en représentation : simulateur actif, zone éteinte, attracteur actif, fond enlevé, ambiance forcée, trop de bulles…"))
	void CheckBeforeShow();

	// =====================================================  Mood (quick settings)
	//
	// What the get-in needs, and nothing else, first in the Details panel. The
	// rest of the category is under the advanced arrow.

	/**
	 * Palettes and energy for the three moods. Dropping a preset here loads it
	 * into the stage's own copy below; "Save to preset" writes a tuning back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VibH2O|Ambiance", meta = (DisplayName = "Preset d'ambiance", ToolTip = "Couleurs et énergie des trois ambiances. Glisser un preset ici le charge ; « Enregistrer dans le preset » y réécrit les réglages."))
	TObjectPtr<UVibH2OMoodPreset> MoodPreset;

	/**
	 * Below this mean arousal the room turns calm. Read against "Room arousal"
	 * just below, which shows the live value to set it from.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Seuil calme", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Sous cette excitation moyenne, la salle passe en calme. À régler en regardant « Excitation de la salle » juste en dessous."))
	float CalmThreshold = 0.25f;

	/** Above this mean arousal the room turns excited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Seuil excité", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Au-dessus de cette excitation moyenne, la salle passe en excitée."))
	float ExcitedThreshold = 0.55f;

	/** Lets the operator force a mood - rehearsal, or a cue in the show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Forcer l'ambiance", ToolTip = "Automatique : la salle décide. Sinon l'ambiance est imposée (répétition, cue, iPad). Remis en automatique par « Remettre en état de début de spectacle »."))
	EVibH2OMoodOverride MoodOverride = EVibH2OMoodOverride::Auto;

	/** The mood the room is in, or fading towards. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "VibH2O|Ambiance", meta = (DisplayName = "Ambiance actuelle"))
	EVibH2OMood CurrentMood = EVibH2OMood::Normal;

	/**
	 * Mean arousal of the room, smoothed as the thresholds see it: the number
	 * to watch while setting them, since the range Max sends is not known in
	 * advance.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "VibH2O|Ambiance", meta = (DisplayName = "Excitation de la salle", ToolTip = "Valeur mesurée en direct (0 à 1) : c'est elle que comparent les seuils."))
	float RoomExcitation = 0.0f;

	/** Overall light of the bubbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Lumière des bulles", ClampMin = "0.0", UIMax = "4.0", ToolTip = "Luminosité générale des bulles. À baisser si elles brûlent en blanc, à monter si elles se fondent dans l'eau."))
	float BubbleBrightness = 1.0f;

	/** Base scale of a bubble at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Ambiance", meta = (DisplayName = "Taille des bulles", ClampMin = "0.01", UIMax = "4.0", ToolTip = "Taille d'une bulle au repos (1 = taille normale)."))
	float BaseScale = 1.0f;

	/**
	 * The palettes and energy the stage actually plays: colour, striations,
	 * motion and size for each mood. Animatable in Sequencer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Palettes par ambiance", ToolTip = "Couleurs, stries, mouvement et taille de chaque ambiance. Animable dans Sequencer."))
	FVibH2OMoodPalette MoodPalette;

	/** How long the room takes to cross over from one mood to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Durée de bascule", ClampMin = "0.0", UIMax = "20.0", ForceUnits = "s", ToolTip = "Temps pour passer d'une ambiance à l'autre."))
	float MoodTransitionSeconds = 4.0f;

	/**
	 * Dead band around each threshold. Leaving a mood takes crossing its
	 * threshold by this much, so a room hovering at the border cannot flicker.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Marge anti-clignotement", ClampMin = "0.0", ClampMax = "0.25", ToolTip = "Marge autour des seuils : une salle à la frontière ne clignote pas d'une ambiance à l'autre."))
	float MoodHysteresis = 0.04f;

	/** Time constant smoothing the room's arousal before it is judged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Lissage de la mesure", ClampMin = "0.0", UIMax = "10.0", ForceUnits = "s", ToolTip = "Plus grand : l'ambiance réagit plus lentement aux variations de la salle."))
	float MoodDetectionSmoothing = 1.5f;

	/**
	 * How much a bubble's own synchrony counts against the room's, when it
	 * falls in step. Zero: the room's synchrony alone. One: each person's own.
	 * In between, a person out of step stands out in a synchronous room.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Ambiance", meta = (DisplayName = "Part de la synchronie individuelle", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "0 : seule la synchronie de la salle compte. 1 : celle de chaque personne. Entre les deux, une personne hors du rythme se remarque."))
	float IndividualCoherenceShare = 0.5f;

	/** Loads a preset into the palettes the stage plays. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Ambiance")
	void ApplyMoodPreset(UVibH2OMoodPreset* Preset);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Ambiance")
	void SetMoodOverride(EVibH2OMoodOverride NewOverride);

	/** Jumps to the mood the room implies right now, without cross-fade. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Appliquer l'ambiance sans fondu", ToolTip = "Passe tout de suite à l'ambiance que la salle indique, sans attendre la bascule."))
	void SnapMood();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Ambiance")
	EVibH2OMood GetCurrentMood() const { return CurrentMood; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Ambiance")
	FVibH2OMoodWeights GetMoodWeights() const { return MoodTracker.GetWeights(); }

	/** Writes the palettes the stage plays back into the preset asset. */
	UFUNCTION(CallInEditor, Category = "VibH2O", meta = (DisplayName = "Enregistrer dans le preset", ToolTip = "Écrit les palettes actuelles dans le preset choisi (il peut servir à d'autres cartes)."))
	void SaveMoodPreset();

	/** Reloads the preset, dropping any tuning not saved into it. */
	UFUNCTION(CallInEditor, Category = "VibH2O", meta = (DisplayName = "Recharger le preset", ToolTip = "Recharge le preset : les réglages non enregistrés sont perdus."))
	void ReloadMoodPreset();

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OStageMoodChangedSignature OnMoodChanged;

	// =====================================================  Bubbles

	/** Class instanced for each occupied seat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (DisplayName = "Classe de bulle (Blueprint)", ToolTip = "Le Blueprint de bulle créé pour chaque siège occupé."))
	TSubclassOf<AVibH2OBubbleActor> BubbleClass;

	/** Extra growth at full arousal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0", DisplayName = "Croissance avec l'excitation", UIMax = "1.0", ToolTip = "Grossissement d'une bulle très excitée."))
	float ScalePerExcitation = 0.35f;

	/**
	 * Amplitude of the "pop" on each beat. Kept small: a living thing swells
	 * with its pulse, it does not bounce like a cartoon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0", DisplayName = "Gonflement au battement", ClampMax = "1.0", UIMax = "0.4", ToolTip = "Petit sursaut à chaque battement de cœur. Rester discret : vivant, pas cartoon."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0", ClampMax = "0.6", DisplayName = "Déformation organique", UIMax = "0.15", ToolTip = "Les bulles respirent et ne sont jamais des sphères parfaites. Réaliste sous 0,1 ; au-delà, effet caoutchouc."))
	float OrganicDeform = 0.035f;

	/** How fast that deformation breathes, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0", DisplayName = "Vitesse de la déformation", UIMax = "2.0", ToolTip = "Respirations par seconde."))
	float OrganicSpeed = 0.42f;

	/** Converts the measured speed (cm/s) into striation speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Bulles", meta = (ClampMin = "0.0", DisplayName = "Stries : vitesse selon le déplacement", UIMax = "0.05", ToolTip = "Plus grand : les stries défilent plus vite quand la bulle se déplace."))
	float StriationSpeedScale = 0.01f;

	// =====================================================  Geometry

	/** Lateral gap between two neighbouring seats, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "1.0", ForceUnits = "cm", DisplayName = "Écart entre sièges", UIMin = "40", UIMax = "400", ToolTip = "Distance entre deux sièges voisins d'une rangée."))
	float ColumnSpacing = 120.0f;

	/** Depth gap between two rows, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "1.0", ForceUnits = "cm", DisplayName = "Écart entre rangées", UIMin = "40", UIMax = "400", ToolTip = "Distance entre deux rangées."))
	float RowSpacing = 140.0f;

	/**
	 * SIGNED angle per column step, in degrees. Zero gives an exactly straight
	 * room; positive and negative curve it either way.
	 *
	 * The angle is set per seat rather than as a radius because a straight room
	 * would correspond to an infinite radius, impossible to type into a field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "-30.0", ClampMax = "30.0", DisplayName = "Courbure de la salle (° par siège)", ToolTip = "0 = salle droite. Positif ou négatif : courbe d'un côté ou de l'autre."))
	float CurvatureAngle = 0.0f;

	/**
	 * True: constant angle per seat, so the back rows are wider - the geometry
	 * of a real theatre. False: constant lateral gap everywhere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (DisplayName = "Élargir les rangées du fond", ToolTip = "Les rangées du fond s'ouvrent en éventail, comme dans un théâtre."))
	bool bFanOut = true;

	/**
	 * Bubbles rotate to face the centre of curvature.
	 *
	 * This is not decoration: the bubbles carry directional striations. Were
	 * they all to keep the same heading on a curved room, the striations would
	 * all point the same way and give the curvature away at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Geometrie", meta = (DisplayName = "Tourner les bulles vers le centre", ToolTip = "Les stries des bulles suivent la courbe de la salle."))
	bool bOrientToCenter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Geometrie", meta = (DisplayName = "Rotation supplémentaire", ToolTip = "Tourne toutes les bulles d'un même angle."))
	FRotator OrientationOffset = FRotator::ZeroRotator;

	/** Vertical rise per row, to reproduce raked seating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ForceUnits = "cm", DisplayName = "Gradins : hauteur par rangée", UIMin = "-100", UIMax = "200", ToolTip = "Chaque rangée monte d'autant, comme des gradins. 0 = salle à plat."))
	float ElevationPerRow = 0.0f;

	/** Optional curve for a non-linear rake, evaluated over [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Geometrie", meta = (DisplayName = "Gradins : courbe (facultatif)", ToolTip = "Courbe de hauteur des rangées, si les gradins ne sont pas réguliers."))
	TObjectPtr<UCurveFloat> ElevationCurve;

	/** Amplitude of the depth offset, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Geometrie", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Relief : amplitude", UIMax = "300", ToolTip = "Petites variations de hauteur pour que la salle ne soit pas parfaitement plate. 0 = plat."))
	float DepthAmplitude = 0.0f;

	/**
	 * Scale of the noise producing that offset. The relief comes from noise and
	 * not from per-seat randomness: pure randomness would decorrelate
	 * neighbours and destroy the reading of the rows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Geometrie", meta = (ClampMin = "0.01", DisplayName = "Relief : finesse", UIMax = "2", ToolTip = "Petit : grandes ondulations. Grand : relief serré."))
	float DepthNoiseScale = 0.25f;

	// =====================================================  Movement

	/** Amplitude of the permanent float, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Flottement : amplitude", UIMax = "100", ToolTip = "Les bulles flottent doucement sur place, comme dans l'eau."))
	float FloatAmplitude = 12.0f;

	/** Float speed, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", DisplayName = "Flottement : vitesse", UIMax = "2", ToolTip = "Vitesse du flottement."))
	float FloatSpeed = 0.35f;

	/** Amplitude of the slow pitch that accompanies the float, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", DisplayName = "Flottement : bascule", UIMax = "45", ToolTip = "Les bulles se balancent d'autant de degrés en flottant."))
	float FloatRotationAmount = 6.0f;

	/** Below this arousal threshold, complete stillness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ClampMax = "0.99", DisplayName = "Dérive : à partir de l'excitation", ToolTip = "Une bulle commence à quitter sa place au-dessus de cette excitation."))
	float DriftThreshold = 0.4f;

	/** Drift amplitude at full arousal, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Dérive : amplitude max", UIMax = "200", ToolTip = "Distance maximale qu'une bulle très excitée parcourt autour de sa place."))
	float DriftIntensity = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", DisplayName = "Dérive : changements de direction", UIMax = "1", ToolTip = "Plus grand : la dérive change de direction plus souvent."))
	float DriftDirectionSpeed = 0.12f;

	/**
	 * Fraction of the seat spacing the drift may not exceed. The maximum is
	 * deliberately capped at 0.49: below half a cell, a bubble mathematically
	 * cannot reach its neighbour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Mouvement", meta = (ClampMin = "0.0", ClampMax = "0.49", DisplayName = "Dérive : limite (part de l'écart)", ToolTip = "Une bulle ne s'éloigne jamais de plus de cette part de l'écart entre sièges : elle ne touche jamais sa voisine."))
	float DriftMaxRatio = 0.35f;

	// =====================================================  Tableaux

	/**
	 * Morph from tableau 1 to tableau 2, 0 to 1.
	 *
	 * Both drivers run permanently; this slider only interpolates between them.
	 * The transition is therefore continuous, reversible and stoppable half
	 * way, with no state machine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Tableau : salle (0) → banc (1)", ToolTip = "0 = plan de salle, 1 = banc / vortex. Continu, réversible, peut s'arrêter à mi-chemin. Les bulles se divisent au début du passage."))
	float BlendAlpha = 0.0f;

	/**
	 * Time to reach a value sent by the iPad, Max or a cue. Sequencer and the
	 * Details panel still set BlendAlpha directly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux", meta = (DisplayName = "Transition commandée : durée", ClampMin = "0.0", UIMax = "30.0", ForceUnits = "s", ToolTip = "Temps pour rejoindre la valeur envoyée par l'iPad, Max ou une cue. 0 = saut immédiat."))
	float OscBlendGlideSeconds = 6.0f;

	/** Glides to tableau 1 over the commanded transition time. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Aller au tableau 1 (salle)"))
	void GoToTableau1();

	/** Glides to tableau 2 over the commanded transition time. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Aller au tableau 2 (banc)"))
	void GoToTableau2();

	/** Glides BlendAlpha to Target; Seconds < 0 uses OscBlendGlideSeconds. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Tableaux")
	void GlideBlendTo(float Target, float Seconds = -1.0f);

	/** Easing curve for the morph. Null means linear interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Tableaux", meta = (DisplayName = "Courbe de la transition", ToolTip = "Facultatif : forme du passage salle → banc."))
	TObjectPtr<UCurveFloat> BlendCurve;

	/** Tableau 1 driver. Replaceable by a Blueprint subclass. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Tableaux", meta = (DisplayName = "Pilote du tableau 1 (réglages fins)", ToolTip = "Ne pas changer de classe : cela efface les réglages."))
	TObjectPtr<UVibH2OPositionDriver> GridDriver;

	/**
	 * Tableau 2 driver. Defaults to the boids school; swap in the vortex driver
	 * for the simpler cone.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Tableaux", meta = (DisplayName = "Pilote du tableau 2 (réglages fins)", ToolTip = "Réglages du banc de poissons. Ne pas changer de classe : cela efface les réglages."))
	TObjectPtr<UVibH2OPositionDriver> FlockDriver;

	// -------- Vortex settings, read by the tableau 2 driver.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "1.0", ForceUnits = "cm", DisplayName = "Vortex : hauteur", UIMax = "3000", ToolTip = "Hauteur du cône formé par le banc en synchronie."))
	float VortexHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Vortex : rayon en bas", UIMax = "2000", ToolTip = "Rayon du bas du cône."))
	float VortexBaseRadius = 200.0f;

	/** Larger than the base radius for a cone that opens upward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Vortex : rayon en haut", UIMax = "2000", ToolTip = "Rayon du haut du cône (il s'évase vers le haut)."))
	float VortexTopRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0", UIMax = "10", ToolTip = "Pilote « Vortex simple » seulement — sans effet avec le banc de poissons."))
	float VortexTurns = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (DisplayName = "Vortex : sens de rotation", UIMin = "-1", UIMax = "1", ToolTip = "Positif : sens horaire vu du dessus ; négatif : l'autre sens. Avec le banc de poissons, seul le signe compte."))
	float VortexSpinSpeed = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ToolTip = "Pilote « Vortex simple » seulement — sans effet avec le banc de poissons."))
	float VortexAngleDispersion = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ToolTip = "Pilote « Vortex simple » seulement — sans effet avec le banc de poissons."))
	float VortexRadiusDispersion = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ToolTip = "Pilote « Vortex simple » seulement — sans effet avec le banc de poissons."))
	float VortexHeightDispersion = 0.12f;

	/**
	 * Share of individual synchrony in the scatter. Zero lets collective
	 * synchrony alone set the order, as the piece intends.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Vortex", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Pilote « Vortex simple » seulement — sans effet avec le banc de poissons."))
	float VortexIndividualInfluence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Vortex", meta = (ForceUnits = "cm", DisplayName = "Vortex : décalage vertical", ToolTip = "Monte ou descend tout le vortex par rapport à la salle."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Filles par bulle", ClampMin = "0", ClampMax = "8", ToolTip = "Chaque bulle se divise en (1 + ce nombre) au passage au banc. 176 places × 4 = 704 bulles : si l'image rame, baisser. Un son branché sur « battement » sonne autant de fois (filtrer avec Is Daughter)."))
	int32 DaughtersPerBubble = 3;

	/**
	 * Ceiling on the whole school, mothers included. Past it, fewer daughters
	 * are born rather than the frame rate falling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Plafond du banc", ClampMin = "1", ClampMax = "2000", UIMax = "1200", ToolTip = "Nombre total de bulles à ne jamais dépasser, filles comprises."))
	int32 MaxSchoolSize = 1000;

	/** BlendAlpha at which the bubbles begin to divide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Division : début", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Valeur du tableau où la division commence."))
	float DivisionStart = 0.0f;

	/**
	 * BlendAlpha at which the division is complete. The families leave their
	 * seats half way through it, so the two gestures overlap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Division : fin", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Valeur du tableau où la division est terminée."))
	float DivisionEnd = 0.35f;

	/** How far the sisters stand from their seat once divided, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Division", meta = (DisplayName = "Écart des sœurs", ClampMin = "0.0", ForceUnits = "cm", UIMax = "150", ToolTip = "Distance entre les sœurs juste après la division."))
	float DivisionSpread = 35.0f;

	// -------- Order of the school, read by the tableau 2 boids driver.

	/** Below this collective synchrony the school is fully dispersed into families. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Synchronie : dispersion sous", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Sous cette synchronie collective, le banc s'éparpille (familles lisibles)."))
	float SchoolDisperseBelow = 0.35f;

	/** Above this collective synchrony the whole school mills as one vortex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Synchronie : vortex au-dessus de", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Au-dessus de cette synchronie collective, le banc forme le vortex."))
	float SchoolVortexAbove = 0.70f;

	/**
	 * Seconds the school takes to follow a change of synchrony. Synchrony is a
	 * noisy measurement; without this the vortex would tremble.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Banc : lissage", ClampMin = "0.0", ForceUnits = "s", UIMax = "10", ToolTip = "Temps de réaction du banc aux changements de synchronie."))
	float SchoolOrderSmoothing = 2.0f;

	/**
	 * 1: the room's synchrony decides the order of the school. 0: "Ordre
	 * manuel" decides. In between, a mix - to hand control to the director
	 * for a moment, or to rehearse without an audience.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Part de la synchronie", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "1 : la synchronie décide de la forme du banc. 0 : seul « Ordre manuel » décide."))
	float SchoolSyncInfluence = 1.0f;

	/** Order used instead of the synchrony's: 0 dispersed into families, 1 vortex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Tableaux|Banc", meta = (DisplayName = "Ordre manuel", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Ordre du banc imposé à la main (0 dispersé, 1 vortex), mélangé à la synchronie selon « Part de la synchronie »."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Focus", meta = (DisplayName = "Groupes de focus", TitleProperty = "GroupName", ToolTip = "Groupes de sièges que le focus peut mettre en valeur."))
	TArray<FVibH2OFocusGroup> FocusGroups;

	/** Currently selected group. Empty means no focus at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VibH2O|Focus", meta = (DisplayName = "Groupe de focus actif", GetOptions = "GetFocusGroupNames", ToolTip = "Le groupe mis en valeur. Choisir dans la liste."))
	FName ActiveFocusGroup = NAME_None;

	/**
	 * Focus intensity, 0 to 1. Nothing moves: the room plan stays readable,
	 * only the bubbles outside the group are dimmed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Focus", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Intensité du focus", ToolTip = "0 = toute la salle pareille ; 1 = seul le groupe actif reste allumé."))
	float FocusAmount = 0.0f;

	/** Mask value for a bubble outside the group, at full intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Focus", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Lumière hors du groupe", ToolTip = "Luminosité des bulles hors du groupe quand le focus est à 1."))
	float FocusDimmedValue = 0.12f;

	/** Margin applied to the computed pull-back distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Focus", meta = (ClampMin = "1.0", DisplayName = "Marge de cadrage caméra", UIMax = "2", ToolTip = "Marge laissée autour du groupe quand une caméra se cadre dessus."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Commande", meta = (DisplayName = "Suivre /Blend (Max, iPad, cues)", ToolTip = "Décoché : l'iPad, Max et les cues ne peuvent plus changer de tableau. Sequencer passe toujours."))
	bool bFollowOscBlend = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Commande", meta = (DisplayName = "Suivre /Focus", ToolTip = "Décoché : le focus n'obéit plus à l'OSC ni à l'iPad."))
	bool bFollowOscFocus = true;

	/** Follow /Attractors/FadeScale and /Attractors/Release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Commande", meta = (DisplayName = "Suivre « Tout relâcher » et la durée des fondus", ToolTip = "Décoché : ces deux commandes de l'iPad sont ignorées."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Attracteurs", meta = (ClampMin = "0.1", ClampMax = "10.0", DisplayName = "Multiplicateur des fondus", ToolTip = "Multiplie toutes les durées d'arrivée et de retour des attracteurs (2 = deux fois plus lent)."))
	float AttractorFadeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Attracteurs", meta = (DisplayName = "Appliquer les attracteurs", ToolTip = "Décoché : les attracteurs de la carte sont ignorés."))
	bool bApplyAttractors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Attracteurs", meta = (DisplayName = "Appliquer les zones d'exclusion", ToolTip = "Décoché : les zones d'exclusion sont ignorées. Attention aux circassiennes."))
	bool bApplyExclusionZones = true;

	/** Every attractor that follows OSC lets go, each over its own fade. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Relâcher tous les attracteurs", ToolTip = "Toutes les bulles appelées rentrent à leur place."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Commande", meta = (DisplayName = "Publier l'etat pour l'iPad", ToolTip = "Envoie l'état de la salle à l'iPad. À laisser coché."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Previsualisation", meta = (DisplayName = "Aperçu schématique (sans simulateur)", ToolTip = "Dessine la salle en fil de fer quand aucune donnée n'arrive."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Previsualisation", meta = (DisplayName = "Bulles vivantes dans l'editeur", ToolTip = "Vraies bulles dans l'éditeur, sans Play (viewport en temps réel : Ctrl+R). Elles ne s'enregistrent pas et ne se sélectionnent pas."))
	bool bLiveInEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Previsualisation", meta = (ClampMin = "1", ClampMax = "64", DisplayName = "Aperçu : colonnes", ToolTip = "Taille de l'aperçu, seulement quand aucun simulateur ni Max n'alimente la salle."))
	int32 PreviewColumns = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Previsualisation", meta = (ClampMin = "1", ClampMax = "64", DisplayName = "Aperçu : rangées", ToolTip = "Taille de l'aperçu, seulement quand aucun simulateur ni Max n'alimente la salle."))
	int32 PreviewRows = 3;

	/** Also draws one arrow per seat, to check the orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Previsualisation", meta = (DisplayName = "Aperçu : flèches"))
	bool bPreviewShowOrientation = true;

	/**
	 * Writes each spectator's number over the middle of their bubble.
	 *
	 * For the audience, so each spectator finds themselves in the room; for
	 * the setup, so a wrong seat order is caught in one look rather than on
	 * opening night. Interp: a cue in a Level Sequence can show and hide them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Numéros", meta = (DisplayName = "Afficher les numéros", ToolTip = "Le numéro de chaque spectateur au centre de sa bulle, pour qu'il s'y retrouve. Animable dans Sequencer."))
	bool bShowSeatNumbers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Numéros", meta = (DisplayName = "Numéro affiché", ToolTip = "L'identifiant du spectateur envoyé par Max, ou le numéro du siège dans le plan de salle."))
	EVibH2OSeatNumber SeatNumberSource = EVibH2OSeatNumber::Individual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Numéros", meta = (DisplayName = "Taille des numéros", ClampMin = "0.1", ClampMax = "1.5", UIMin = "0.2", UIMax = "1.0", ToolTip = "Hauteur des chiffres, en part du diamètre de la bulle. 0,55 : lisible sans déborder pour deux ou trois chiffres."))
	float SeatNumberSize = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Numéros", meta = (DisplayName = "Couleur des numéros"))
	FLinearColor SeatNumberColor = FLinearColor::White;

	/**
	 * Animate the preview: the float, and a fake heartbeat pulsing the markers.
	 *
	 * No data exists in the editor - there is no game world, so no subsystem
	 * and no individuals. The preview therefore invents a plausible BPM per
	 * seat, purely so the room reads as alive while the geometry is being
	 * dialled in. It proves nothing about real data; that is what Simulate and
	 * Play are for.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Previsualisation", meta = (DisplayName = "Aperçu : animer", ToolTip = "Fait battre l'aperçu schématique. Sans rapport avec les vraies données."))
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
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Reconstruire la salle", ToolTip = "Recrée toutes les bulles avec les réglages actuels."))
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

	UFUNCTION()
	void HandleOscAmbiance(int32 MoodOverrideIndex);

	/** Choices offered by the "Groupe de focus actif" drop-down. */
	UFUNCTION()
	TArray<FName> GetFocusGroupNames() const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (AllowPrivateAccess = "true"))
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, AdvancedDisplay, Category = "VibH2O|Diagnostic", meta = (AllowPrivateAccess = "true", DisplayName = "Horloge de la scène (s)"))
	float StageTime = 0.0f;

	/**
	 * The subsystem whose delegates we are bound to. Kept rather than looked up
	 * again on unbind: in the editor it is the stand-in, which a world cleanup
	 * may already have replaced.
	 */
	TWeakObjectPtr<UVibH2OSubsystem> BoundSubsystem;

	/** A commanded glide of BlendAlpha in progress (iPad, Max, cue, buttons). */
	bool bBlendGliding = false;
	float BlendGlideFrom = 0.0f;
	float BlendGlideTarget = 0.0f;
	float BlendGlideElapsed = 0.0f;
	float BlendGlideDuration = 0.0f;
	void StepBlendGlide(float DeltaSeconds);

	/** Where the room's data comes from, for the iPad: the network moved lately. */
	int32 LastNetworkDataCount = 0;
	double LastNetworkDataTime = -1000.0;

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
