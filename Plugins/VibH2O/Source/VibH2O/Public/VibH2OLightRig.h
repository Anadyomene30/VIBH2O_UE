// The light rig: three spot lights that keep the bubbles lit, whatever the room.
//
// A light placed by hand frames one room. Resize it from 7 x 3 to the 23 x 4 of
// Ales, curve it, move the stage or morph into the school, and that light now
// hits half the audience or misses it. The rig instead reads where the bubbles
// actually are every frame and re-aims, re-throws and re-powers its lights to
// match:
//
//   * placement - each light keeps its angle relative to the STAGE (so turning
//     the installation turns the lighting with it) and backs off until its cone
//     covers the whole room;
//   * power - intensity is compensated for that distance (inverse square), so
//     a bubble receives the same light in a room of 20 as in a room of 200;
//   * smoothing - every change glides, so a room rebuilt mid-show does not
//     make the lights jump.
//
// Like the demonstration bubble, it is a working default and not an artistic
// direction: every angle, colour and level is in the Details panel and
// animatable in Sequencer, and an artist may delete it for their own lighting.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VibH2OLightRig.generated.h"

class AVibH2OStageActor;
class USpotLightComponent;

/** One light of the rig, described relative to the room it lights. */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2ORigLight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Allumée"))
	bool bEnabled = true;

	/**
	 * Direction the light comes FROM, as a yaw around the stage, in degrees.
	 * 0 is the stage's forward axis; it turns with the stage actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Direction (azimut, °)", ClampMin = "-180.0", ClampMax = "180.0", ToolTip = "D'où vient la lumière, en tournant autour de la scène. 0 = l'axe avant de l'acteur de scène ; elle tourne avec lui."))
	float Azimuth = 0.0f;

	/** Height of the light above the room's horizon, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Hauteur (°)", ClampMin = "-30.0", ClampMax = "89.0", ToolTip = "Hauteur de la lumière au-dessus de l'horizon de la salle, en degrés."))
	float Elevation = 45.0f;

	/**
	 * Light received at the centre of the room, in lux - not the lamp's power.
	 * The rig works the lamp's candelas out from its distance, which is what
	 * keeps the room equally lit at any size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Lumière reçue par les bulles", ClampMin = "0.0", UIMax = "20.0", ForceUnits = "lux", ToolTip = "Lumière reçue au centre de la salle, en lux - pas la puissance de la lampe. Le rig compense la distance : la salle reste éclairée pareil à toute taille."))
	float Illuminance = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Couleur"))
	FLinearColor Color = FLinearColor::White;

	/** Half-angle of the beam, in degrees. Wider is closer and softer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Ouverture du faisceau (°)", ClampMin = "5.0", ClampMax = "80.0", ToolTip = "Demi-angle du faisceau. Plus large : la lampe se rapproche et la lumière s'adoucit."))
	float ConeAngle = 32.0f;

	/** How much the beam shows in the volumetric fog - the visible shaft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumiere", meta = (DisplayName = "Faisceau visible dans le brouillard", ClampMin = "0.0", UIMax = "10.0"))
	float VolumetricScattering = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lumiere", meta = (DisplayName = "Projette des ombres"))
	bool bCastShadows = true;
};

/**
 * Lighting that follows the room.
 */
UCLASS(Blueprintable, BlueprintType, HideCategories = (Actor, Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance), PrioritizeCategories = ("VibH2O"), AutoExpandCategories = ("VibH2O|Lumieres"), meta = (DisplayName = "VibH2O — Rig de lumières"))
class VIBH2O_API AVibH2OLightRig : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OLightRig();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	/** Stage to light. Left empty, the first stage actor of the level is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Lumieres", AdvancedDisplay, meta = (DisplayName = "Acteur de scène à éclairer", ToolTip = "Vide : le premier acteur de scène de la carte."))
	TObjectPtr<AVibH2OStageActor> Stage;

	/**
	 * Light only the active focus group rather than the whole room.
	 *
	 * The lights then converge on the group as the focus moves - a follow
	 * spot for free. Off by default: the focus already dims the others.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Lumieres", meta = (DisplayName = "Suivre le groupe de focus", ToolTip = "Coché : les lumières convergent sur le groupe de focus et le suivent. Décoché : toute la salle reste éclairée."))
	bool bFollowFocus = false;

	/** Master level, multiplying all three lights. Handy to fade in Sequencer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Lumieres", meta = (DisplayName = "Niveau général", ClampMin = "0.0", UIMax = "4.0", ToolTip = "Multiplie les trois lumières. Pratique à animer dans Sequencer."))
	float MasterIntensity = 1.0f;

	/** How fast the lights follow a change of room, per second. 0 snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Lumieres", meta = (DisplayName = "Vitesse de suivi", ClampMin = "0.0", UIMax = "10.0", ToolTip = "Vitesse à laquelle les lumières suivent un changement de salle. 0 = saut immédiat."))
	float FollowSpeed = 1.5f;

	/** Extra room around the bubbles, as a factor of the room's radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Lumieres", AdvancedDisplay, meta = (DisplayName = "Marge de couverture", ClampMin = "1.0", UIMax = "2.0"))
	float CoverageMargin = 1.15f;

	/**
	 * Closest a light may come to the centre, in cm. Keeps a tiny room from
	 * pulling the lamps into the bubbles.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Lumieres", AdvancedDisplay, meta = (DisplayName = "Distance minimale", ClampMin = "50.0", UIMax = "3000.0", ForceUnits = "cm"))
	float MinDistance = 600.0f;

	/**
	 * The key: the main light, from in front and above, slightly warm. It
	 * gives the bubbles their shape and their highlight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Lumieres", meta = (DisplayName = "Face (key)"))
	FVibH2ORigLight Key;

	/**
	 * The back light: from behind, cold and strong. It is the one that makes a
	 * subsurface bubble glow from inside and draws a bright rim round it -
	 * and, through the fog, a visible shaft.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Lumieres", meta = (DisplayName = "Contre-jour (back)"))
	FVibH2ORigLight Back;

	/** The fill: low, from the other side, deep blue. It keeps the shadows from going black. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Lumieres", meta = (DisplayName = "Débouchage (fill)"))
	FVibH2ORigLight Fill;

	/** Centre and radius currently lit, in world space, after smoothing. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Lumieres")
	FVector GetLitCenter() const { return SmoothedCenter; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Lumieres")
	float GetLitRadius() const { return SmoothedRadius; }

	/** Re-aims at once, without smoothing. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Recadrer les lumières maintenant"))
	void SnapToRoom();

private:
	AVibH2OStageActor* ResolveStage();
	void UpdateRig(float DeltaSeconds, bool bSnap);
	void ApplyLight(USpotLightComponent* Component, const FVibH2ORigLight& Settings, const FQuat& StageRotation) const;

	UPROPERTY(VisibleAnywhere, Category = "VibH2O|Lumieres", meta = (ToolTip = "Réglé par le rig à chaque image — régler plutôt Face / Contre-jour / Débouchage."))
	TObjectPtr<USpotLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "VibH2O|Lumieres", meta = (ToolTip = "Réglé par le rig à chaque image — régler plutôt Face / Contre-jour / Débouchage."))
	TObjectPtr<USpotLightComponent> BackLight;

	UPROPERTY(VisibleAnywhere, Category = "VibH2O|Lumieres", meta = (ToolTip = "Réglé par le rig à chaque image — régler plutôt Face / Contre-jour / Débouchage."))
	TObjectPtr<USpotLightComponent> FillLight;

	FVector SmoothedCenter = FVector::ZeroVector;
	float SmoothedRadius = 0.0f;
	bool bHasTarget = false;
};
