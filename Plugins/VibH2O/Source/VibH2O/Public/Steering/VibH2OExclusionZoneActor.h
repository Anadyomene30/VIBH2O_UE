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
 * half width and half height; its X extent only matters with bLimitDepth.
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "VibH2O — Zone d'exclusion"))
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

	/** Name used by the iPad and in OSC: /Exclusion/<name>/. From the label when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Nom de controle"))
	FName ControlName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Nom affiche"))
	FString DisplayLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Couleur"))
	FLinearColor DisplayColor = FLinearColor(0.6f, 0.05f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Ordre d'affichage"))
	int32 DisplayOrder = 0;

	/**
	 * Target activation, 0 to 1. On by default: a zone placed in the level
	 * protects as soon as it is there. The push itself fades over FadeSeconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Pilotage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Activation"))
	float Activation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Pilotage", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Duree du fondu"))
	float FadeSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Pilotage", meta = (DisplayName = "Suivre l'OSC et l'iPad"))
	bool bFollowOsc = true;

	// =====================================================  Shape

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Forme", meta = (DisplayName = "Forme"))
	EVibH2OExclusionShape Shape = EVibH2OExclusionShape::Ellipse;

	/**
	 * Soft band round the shape, in cm. Bubbles entering it are eased toward
	 * the edge instead of stopping dead. Count a bubble's radius in it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Forme", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Marge douce"))
	float Margin = 80.0f;

	/**
	 * How fast a bubble caught inside may swing round to its new side, in
	 * degrees per second. A bubble whose path crosses the zone glides round
	 * its edge at this speed instead of jumping across.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Forme", meta = (ClampMin = "0.0", DisplayName = "Vitesse de contournement"))
	float GlideDegreesPerSecond = 150.0f;

	/**
	 * Off: the zone reaches through the whole depth of the scene - what lands
	 * on the performer in the image, whatever its distance. On: only bubbles
	 * within the box's X extent are pushed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Forme", meta = (DisplayName = "Limiter en profondeur"))
	bool bLimitDepth = false;

	/**
	 * Optional: the camera that films the projection. Given one, the zone
	 * becomes a true cone from that camera, so it covers the performer in the
	 * image at every depth, with perspective. Without one, the zone is pushed
	 * straight along its X arrow, which is right for a distant camera.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Forme", meta = (DisplayName = "Camera de projection"))
	TObjectPtr<AActor> ViewCamera;

	// =====================================================  Live drag

	/**
	 * Live position, -1 to 1, across DragRange. The iPad pad sends this; 0 0
	 * is where the zone was placed - the middle of the tulle, typically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Deplacement", meta = (ClampMin = "-1.0", ClampMax = "1.0", DisplayName = "Position X"))
	float DragX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Deplacement", meta = (ClampMin = "-1.0", ClampMax = "1.0", DisplayName = "Position Y"))
	float DragY = 0.0f;

	/**
	 * Half range of the live position, in cm, along the zone's Y and Z. The
	 * default covers a tulle of 8 m by 6 m from its centre.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Deplacement", meta = (DisplayName = "Amplitude (demi-largeur, demi-hauteur)"))
	FVector2D DragRange = FVector2D(400.0, 300.0);

	/** The zone follows the finger with this lag, in seconds - no jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Deplacement", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Lissage"))
	float DragSmoothingSeconds = 0.4f;

	/** Mirrors left and right on the iPad, for a zone placed facing the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Deplacement", meta = (DisplayName = "Inverser gauche / droite"))
	bool bMirrorDrag = false;

	// =====================================================  Blueprint

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Pilotage")
	void SetActive(bool bActive);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Pilotage", meta = (DisplayName = "Basculer"))
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Pilotage")
	bool IsActive() const { return Activation > 0.5f; }

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Deplacement")
	void SetDragPosition(float X, float Y);

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
