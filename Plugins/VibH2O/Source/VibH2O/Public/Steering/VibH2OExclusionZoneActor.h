// An exclusion zone: a place in the image the bubbles must leave clear.
//
// Made for the aerialists: they perform in front of the projection tulle, and
// a bubble drifting across a performer's body reads as a projection mistake.
// The zone is a 2D shape seen along its X arrow - or from a camera, when one is
// designated - and it reaches through the whole depth of the scene, since a
// bubble far behind a performer still lands on her in the image.
//
// It is the very last layer of the position stack, after the tableaux, the
// attractors, the float and the drift: nothing added later can carry a bubble
// back in.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Steering/VibH2OSteeringMath.h"

#include "VibH2OExclusionZoneActor.generated.h"

class UArrowComponent;
class UBoxComponent;

UENUM(BlueprintType)
enum class EVibH2OExclusionShape : uint8
{
	Ellipse UMETA(DisplayName = "Ellipse"),
	/** A box with rounded corners, so no bubble snags on a corner. */
	Rectangle UMETA(DisplayName = "Rectangle arrondi")
};

/**
 * A zone the bubbles are pushed out of, which can be slid about live from the
 * iPad to follow a performer.
 *
 * Place it where the performer is seen, its X arrow pointing the way the
 * camera looks - toward the tulle. The box's Y and Z extents are the shape's
 * half width and half height, mirrored by HalfSize so they can be set without
 * selecting the component; its X extent only matters with bLimitDepth.
 */
UCLASS(Blueprintable, BlueprintType,
	HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance),
	PrioritizeCategories = ("VibH2O"),
	meta = (DisplayName = "VibH2O — Zone d'exclusion"))
class VIBH2O_API AVibH2OExclusionZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OExclusionZoneActor();

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// =====================================================  Control

	/**
	 * Name used by the iPad and in OSC: /Exclusion/<name>/. From the label when
	 * empty. Spaces and slashes typed in the Details panel become '_'.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Nom de contrôle",
			ToolTip = "Nom utilisé par l'iPad et l'OSC : /Exclusion/<nom>/. Vide : repris du nom de l'acteur. Les espaces et les « / » deviennent « _ »."))
	FName ControlName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Nom affiché", ToolTip = "Texte de la zone sur l'iPad. Vide : le nom de contrôle."))
	FString DisplayLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Couleur sur l'iPad",
			ToolTip = "Couleur de la zone sur l'iPad, pour distinguer les zones dans le noir. Teinte aussi la boîte dans le viewport."))
	FLinearColor DisplayColor = FLinearColor(0.6f, 0.05f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Ordre d'affichage", ToolTip = "Les zones de l'iPad sont rangées selon ce numéro, puis par nom."))
	int32 DisplayOrder = 0;

	/**
	 * Target activation, 0 to 1. On by default: a zone placed in the level
	 * protects as soon as it is there. The push itself fades over FadeSeconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Pilotage",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Activation",
			ToolTip = "Éteinte, les bulles peuvent passer devant la circassienne. Toujours la rallumer avant de sauvegarder."))
	float Activation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Pilotage",
		meta = (ClampMin = "0.0", UIMax = "10.0", ForceUnits = "s", DisplayName = "Durée du fondu",
			ToolTip = "Temps pour que la poussée s'installe ou s'éteigne quand on allume ou éteint la zone. 0 = saut immédiat."))
	float FadeSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage",
		meta = (DisplayName = "Suivre l'OSC et l'iPad",
			ToolTip = "Décoché : la zone ignore l'iPad et les messages /Exclusion (allumage et déplacement). Seuls Sequencer, les cues et les Blueprints la pilotent alors."))
	bool bFollowOsc = true;

	// =====================================================  Shape

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Forme",
		meta = (DisplayName = "Forme", ToolTip = "Ellipse, ou rectangle aux coins arrondis pour qu'aucune bulle ne s'accroche à un coin."))
	EVibH2OExclusionShape Shape = EVibH2OExclusionShape::Ellipse;

	/**
	 * Half width and half height of the shape, in cm, before the actor's
	 * scale: the box component's Y and Z extents, settable here without
	 * selecting the component.
	 *
	 * Kept in step with the component both ways (see SyncHalfSize): a map
	 * saved before this field existed, or a box resized on the component
	 * itself, hands its size to this field rather than being overwritten.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Forme",
		meta = (DisplayName = "Demi-largeur, demi-hauteur", ClampMin = "10.0", ForceUnits = "cm",
			ToolTip = "Demi-largeur et demi-hauteur de la zone, en cm, avant l'échelle de l'acteur. C'est la taille de la boîte dans le viewport."))
	FVector2D HalfSize = FVector2D(90.0, 130.0);

	/**
	 * Soft band round the shape, in cm. Bubbles entering it are eased toward
	 * the edge instead of stopping dead. Count a bubble's radius in it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Forme",
		meta = (ClampMin = "0.0", UIMax = "300.0", ForceUnits = "cm", DisplayName = "Marge douce",
			ToolTip = "Bande douce autour de la forme, en cm : les bulles qui y entrent sont guidées vers le bord au lieu de s'arrêter net. Y compter le rayon d'une bulle."))
	float Margin = 80.0f;

	/**
	 * How fast a bubble caught inside may swing round to its new side, in
	 * degrees per second. A bubble whose path crosses the zone glides round
	 * its edge at this speed instead of jumping across.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category = "VibH2O|Forme",
		meta = (ClampMin = "10.0", UIMax = "720.0", DisplayName = "Vitesse de contournement",
			ToolTip = "Vitesse, en degrés par seconde, à laquelle une bulle prise dans la zone en fait le tour pour passer de l'autre côté, au lieu de sauter."))
	float GlideDegreesPerSecond = 150.0f;

	/**
	 * Off: the zone reaches through the whole depth of the scene - what lands
	 * on the performer in the image, whatever its distance. On: only bubbles
	 * within the box's X extent are pushed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Forme",
		meta = (DisplayName = "Limiter en profondeur",
			ToolTip = "Décoché : la zone traverse toute la profondeur de la scène, ce qui protège la circassienne à l'image quelle que soit la distance. Coché : seules les bulles dans l'épaisseur X de la boîte sont poussées."))
	bool bLimitDepth = false;

	/**
	 * Optional: the camera that films the projection. Given one, the zone
	 * becomes a true cone from that camera, so it covers the performer in the
	 * image at every depth, with perspective. Without one, the zone is pushed
	 * straight along its X arrow, which is right for a distant camera.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Forme",
		meta = (DisplayName = "Caméra de projection", AllowedClasses = "/Script/Engine.CameraActor",
			ToolTip = "Facultatif : la caméra qui filme la projection. La zone devient alors un cône depuis cette caméra et couvre la circassienne à l'image à toute profondeur. Vide : la zone est poussée droit le long de sa flèche X."))
	TObjectPtr<AActor> ViewCamera;

	// =====================================================  Live drag

	/**
	 * Live position, -1 to 1, across DragRange. The iPad pad sends this; 0 0
	 * is where the zone was placed - the middle of the tulle, typically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Déplacement",
		meta = (ClampMin = "-1.0", ClampMax = "1.0", DisplayName = "Position X",
			ToolTip = "Position en direct, de -1 à 1 sur l'amplitude. Le pavé de l'iPad l'envoie ; 0 = là où la zone a été placée."))
	float DragX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Déplacement",
		meta = (ClampMin = "-1.0", ClampMax = "1.0", DisplayName = "Position Y",
			ToolTip = "Position en direct, de -1 à 1 sur l'amplitude. Le pavé de l'iPad l'envoie ; 0 = là où la zone a été placée."))
	float DragY = 0.0f;

	/**
	 * Half range of the live position, in cm, along the zone's Y and Z. The
	 * default covers a tulle of 8 m by 6 m from its centre.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Déplacement",
		meta = (DisplayName = "Amplitude (demi-largeur, demi-hauteur)", ClampMin = "10.0", ForceUnits = "cm",
			ToolTip = "Jusqu'où le pavé de l'iPad emmène la zone depuis sa place, en cm. Par défaut, un tulle de 8 m sur 6 m depuis son centre."))
	FVector2D DragRange = FVector2D(400.0, 300.0);

	/** The zone follows the finger with this lag, in seconds - no jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Déplacement",
		meta = (ClampMin = "0.0", UIMax = "2.0", ForceUnits = "s", DisplayName = "Lissage",
			ToolTip = "Retard avec lequel la zone suit le doigt, en secondes : pas de saccades."))
	float DragSmoothingSeconds = 0.4f;

	/** Mirrors left and right on the iPad, for a zone placed facing the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Déplacement",
		meta = (DisplayName = "Inverser gauche / droite", ToolTip = "Inverse gauche et droite sur l'iPad, pour une zone placée face à la caméra."))
	bool bMirrorDrag = false;

	// =====================================================  Blueprint

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Pilotage")
	void SetActive(bool bActive);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Pilotage", meta = (DisplayName = "Basculer"))
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	bool IsActive() const { return Activation > 0.5f; }

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Déplacement")
	void SetDragPosition(float X, float Y);

	/** Brings the live position back to where the zone was placed. A button in the Details panel. */
	UFUNCTION(CallInEditor, Category = "VibH2O|Déplacement", meta = (DisplayName = "Recentrer"))
	void Recenter();

	/** Current push strength, 0 to 1, once faded. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	float GetCurrentWeight() const { return Weight.Value; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	FName GetEffectiveControlName() const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	FString GetEffectiveLabel() const;

	/** Half width and half height of the shape, in cm. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Forme")
	FVector2D GetHalfSize() const;

	// =====================================================  Stage side

	/** Steps the fade and the drag, and caches the zone's frame for the stage. */
	void BeginSteeringFrame(const FTransform& StageRootTransform, float DeltaSeconds);

	/** Pushes one bubble out, in the stage's local space. */
	FVector ApplyToBubble(int32 SeatIndex, int32 Seed, const FVector& LocalLocation);

	void EndSteeringFrame();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O")
	TObjectPtr<USceneComponent> ZoneRoot;

	/** Moves with the live drag; its extent is the shape. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O")
	TObjectPtr<UBoxComponent> Volume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O")
	TObjectPtr<UArrowComponent> Arrow;

private:
#if WITH_EDITOR
	void RefreshAutoControlName();
#endif

	/** Places the volume at the drag offset given, in cm. */
	void ApplyDragOffset(const FVector2D& OffsetCm);
	FVector2D GetDragTargetCm() const;

	/**
	 * Keeps HalfSize and the box extent in step. Whichever moved since the
	 * last sync wins - the component first, so that a size already saved on
	 * it is adopted, never overwritten. bRecordUndo: the component is
	 * Modify()'d before it is resized, so that undoing a HalfSize edit undoes
	 * the box too.
	 */
	void SyncHalfSize(bool bRecordUndo);

	/**
	 * The half size last written to (or adopted from) the box. Starts at an
	 * impossible value, so that the first sync of a map saved before HalfSize
	 * existed adopts the component's extent.
	 */
	UPROPERTY()
	FVector2D LastAppliedHalfSize = FVector2D(-1.0, -1.0);

	/** The name last given from the label: a ControlName still equal to it follows the label. */
	UPROPERTY()
	FName LastAutoControlName = NAME_None;

	FVibH2OSpring Weight;
	FVibH2OSpring DragSpringX;
	FVibH2OSpring DragSpringY;
	bool bDragInitialised = false;

	struct FBubbleEntry
	{
		FVibH2OZoneBubbleState State;
		uint64 LastFrame = 0;
	};
	TMap<int32, FBubbleEntry> BubbleStates;

	// The frame, in stage space, cached by BeginSteeringFrame.
	bool bPerspective = false;
	FVector FrameOrigin = FVector::ZeroVector;
	FVector FrameForward = FVector::ForwardVector;
	FVector FrameRight = FVector::RightVector;
	FVector FrameUp = FVector::UpVector;
	float FrameReferenceDepth = 1.0f;
	float FrameHalfDepth = 0.0f;
	FVibH2OZonePushParams FrameParams;
	float FrameDeltaSeconds = 0.0f;
	uint64 FrameNumber = 0;
	uint64 LastSteppedFrame = 0;
};
