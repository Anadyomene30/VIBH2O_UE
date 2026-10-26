// The control desk: the few settings a show needs, in one place.
//
// The stage, the water, the exposure and the camera each carry dozens of
// settings, which is right for building the piece and wrong for running it:
// in rehearsal there is no time to remember which actor holds which slider.
// The desk gathers the ones that matter on the night - light, colours,
// speeds, how much the sensors move the bubbles, the camera's descent - with
// plain French names, bounded sliders, all animatable in Sequencer.
//
// It owns none of what it drives. It pushes its values into the actors that
// do the work, only when a value changes, so the fine settings left on those
// actors stay theirs. The same values are published to the tablet, which
// builds its tabs from a table the desk serves, and can be saved as named
// presets that survive a relaunch.
//
// The camera stays the director's. The desk works out the rail position that
// frames every occupied seat and writes it where the director's camera
// controller reads its end point, so the keyboard keys keep working; it moves
// the rail itself only when the tablet asks, and steps aside the moment
// anything else moves it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VibH2ORegie.generated.h"

class ACameraRig_Rail;
class AExponentialHeightFog;
class APostProcessVolume;
class AVibH2OStageActor;
class UFXSystemComponent;
class FJsonObject;
struct FVibH2OOscMessage;

/** The surface the picture is projected on: it fixes the camera's format and the Spout resolution. */
UENUM(BlueprintType)
enum class EVibH2OOutputFormat : uint8
{
	TulleFace UMETA(DisplayName = "Tulle face - 1920 x 1440 (4:3)"),
	BacFace UMETA(DisplayName = "Bac face - 1920 x 410 (4,68:1)"),
	TulleCote UMETA(DisplayName = "Tulle cour ou jardin - 960 x 1920 (1:2)"),
	Ecran169 UMETA(DisplayName = "Écran 16:9 - 1920 x 1080"),
	Libre UMETA(DisplayName = "Libre (ne touche ni à la caméra ni à Spout)"),
};

/**
 * The settings of the desk. One struct, so a preset is one JSON object and
 * the tablet's list of controls is one table.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2ORegieSettings
{
	GENERATED_BODY()

	// ------------------------------------------------------------- Spectacle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Luminosité générale (EV)", ClampMin = "4.0", ClampMax = "16.0", ToolTip = "Exposition de toute l'image, de haut en bas du rail. +1 = deux fois plus clair."))
	float Exposure = 10.0f;

	/** The projection surface: the camera takes its format, Spout its resolution, and the framing follows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectacle", meta = (DisplayName = "Format de sortie", ToolTip = "La surface projetée. Fixe le format de la caméra du rail et la résolution envoyée à Resolume par Spout ; le cadrage automatique en tient compte."))
	EVibH2OOutputFormat OutputFormat = EVibH2OOutputFormat::TulleFace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Numéros des spectateurs", ToolTip = "Le numéro de chaque spectateur au centre de sa bulle."))
	bool bShowSeatNumbers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Cadrage : marge", ClampMin = "0.0", ClampMax = "0.8", ToolTip = "Espace laissé autour de la salle à l'arrivée de la caméra. 0 = bord à bord. La fin du rail s'adapte au nombre de spectateurs."))
	float FramingMargin = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Descente : durée (s)", ClampMin = "1.0", ClampMax = "300.0", ToolTip = "Durée de la descente lointaine (touche 0)."))
	float DescentSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Descente : s'arrête à", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Position sur le rail (0 = tout en haut, 1 = tout en bas) où finit la descente lointaine."))
	float ApproachPosition = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Arrivée : durée (s)", ClampMin = "1.0", ClampMax = "120.0", ToolTip = "Durée de l'arrivée sur la salle (touche 1)."))
	float ArrivalSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Particules : sortent du brouillard", ToolTip = "Les particules du rail sont absentes au départ et apparaissent peu à peu pendant la descente."))
	bool bParticlesFadeIn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Particules : apparaissent à", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Position sur le rail où les particules commencent à naître."))
	float ParticlesAppearAt = 0.04f;

	// --------------------------------------------------------------- Ambiance

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Calme : couleur des plus calmes", HideAlphaChannel))
	FLinearColor CalmColorLow = FLinearColor(0.02f, 0.10f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Calme : couleur des plus agités", HideAlphaChannel))
	FLinearColor CalmColorHigh = FLinearColor(0.05f, 0.30f, 0.60f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Normal : couleur des plus calmes", HideAlphaChannel))
	FLinearColor NormalColorLow = FLinearColor(0.02f, 0.30f, 0.38f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Normal : couleur des plus agités", HideAlphaChannel))
	FLinearColor NormalColorHigh = FLinearColor(0.10f, 0.62f, 0.55f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Excité : couleur des plus calmes", HideAlphaChannel))
	FLinearColor ExcitedColorLow = FLinearColor(0.80f, 0.40f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Excité : couleur des plus agités", HideAlphaChannel))
	FLinearColor ExcitedColorHigh = FLinearColor(0.95f, 0.15f, 0.10f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Lumière des bulles", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "À monter si les bulles se fondent dans l'eau, à baisser si elles brûlent en blanc."))
	float BubbleBrightness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Couleur de l'eau", HideAlphaChannel))
	FLinearColor WaterColor = FLinearColor(0.07f, 0.47f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Lumière de l'eau", ClampMin = "0.0", ClampMax = "20.0", ToolTip = "Clarté de l'eau elle-même, derrière les bulles."))
	float WaterLight = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Eau trouble", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Densité de l'eau. Plus trouble : les bulles apparaissent plus tard dans la descente et se voient moins de loin."))
	float WaterDensity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Bascule d'ambiance : durée (s)", ClampMin = "0.0", ClampMax = "30.0"))
	float MoodTransitionSeconds = 4.0f;

	// ----------------------------------------------------------------- Bulles

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Taille des bulles", ClampMin = "0.1", ClampMax = "4.0"))
	float BubbleSize = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Flottement : ampleur", ClampMin = "0.0", ClampMax = "200.0", ToolTip = "De combien les bulles bougent autour de leur place, en cm."))
	float FloatAmplitude = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Flottement : vitesse", ClampMin = "0.0", ClampMax = "5.0"))
	float FloatSpeed = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Battement : gonflement", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Combien la bulle gonfle à chaque battement de cœur."))
	float BeatPop = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Déformation organique", ClampMin = "0.0", ClampMax = "1.0"))
	float OrganicDeform = 0.1f;

	// ------------------------------------------------- Réaction aux capteurs

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Excitation : grossissement", ClampMin = "0.0", ClampMax = "3.0", ToolTip = "Combien une bulle grossit quand son spectateur s'excite."))
	float ScalePerExcitation = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Excitation : dérive", ClampMin = "0.0", ClampMax = "500.0", ToolTip = "De combien une bulle très excitée s'éloigne de sa place, en cm."))
	float DriftIntensity = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Contraste entre spectateurs", ClampMin = "0.0", ClampMax = "6.0", ToolTip = "0 : toutes les bulles de la même couleur. Plus haut : chacun prend sa couleur selon son écart à la salle."))
	float IndividualContrast = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Seuil calme", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Sous ce niveau d'excitation de la salle : ambiance calme."))
	float CalmThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Seuil excité", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Au-dessus de ce niveau : ambiance excitée."))
	float ExcitedThreshold = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Synchronie : couleurs qui se rejoignent", ClampMin = "0.0", ClampMax = "1.0"))
	float SyncHueTightening = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Synchronie : halo", ClampMin = "0.0", ClampMax = "3.0"))
	float SyncHalo = 1.0f;
};

/** One control as the tablet shows it. Kept outside the editor-only metadata: a packaged game has none. */
struct VIBH2O_API FVibH2ORegieControl
{
	const TCHAR* Id;
	const TCHAR* Label;
	const TCHAR* Tab;
	float Min;
	float Max;
	float Step;
};

UENUM(BlueprintType)
enum class EVibH2ORegieCameraMove : uint8
{
	None UMETA(DisplayName = "Immobile"),
	Descent UMETA(DisplayName = "Descente"),
	Arrival UMETA(DisplayName = "Arrivée"),
	Glide UMETA(DisplayName = "Déplacement"),
};

UCLASS(meta = (DisplayName = "VibH2O — Régie"),
	HideCategories = (Actor, Rendering, Replication, Collision, HLOD, Input, LOD, Cooking, Physics, Networking, DataLayers, LevelInstance, WorldPartition),
	PrioritizeCategories = ("VibH2O"),
	AutoExpandCategories = ("VibH2O|Réglages"))
class VIBH2O_API AVibH2ORegie : public AActor
{
	GENERATED_BODY()

public:
	AVibH2ORegie();

	/** What the desk sets. Change it here, from the tablet, or in Sequencer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Réglages", meta = (ShowOnlyInnerProperties))
	FVibH2ORegieSettings Reglages;

	// ---------------------------------------------------------- Connections

	/** Empty: the first stage of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Scène (vide = la première)"))
	TObjectPtr<AVibH2OStageActor> Stage;

	/** Empty: the volume named VibH2O_Exposition, else the first bounded one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Volume d'exposition (vide = VibH2O_Exposition)"))
	TObjectPtr<APostProcessVolume> ExposureVolume;

	/** Empty: the first visible height fog of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Eau (brouillard, vide = le premier)"))
	TObjectPtr<AExponentialHeightFog> Water;

	/** Empty: the first camera rail of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Rail de la caméra (vide = le premier)"))
	TObjectPtr<ACameraRig_Rail> Rail;

	/**
	 * The director's camera controller, whose end points the desk keeps up to
	 * date. Empty: the first actor whose class name contains the text below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Contrôleur de caméra (vide = Camera_Controller)"))
	TObjectPtr<AActor> CameraController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Contrôleur : nom de classe cherché"))
	FString CameraControllerClassName = TEXT("Camera_Controller");

	/** Write the framed end point and the durations into the camera controller, so the keys follow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Mettre à jour le contrôleur de caméra"))
	bool bDriveCameraController = true;

	/** Particle actors and components whose name contains this text appear through the fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Particules : nom cherché"))
	FString ParticleNameFilter = TEXT("Dirt");

	// ------------------------------------------------------------- Readouts

	/** Rail position where the whole room is framed, worked out from the occupied seats. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Cadrage : position calculée"))
	float FramedRailPosition = -1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Caméra : mouvement"))
	EVibH2ORegieCameraMove CameraMove = EVibH2ORegieCameraMove::None;

	// -------------------------------------------------------------- Actions

	/** Pixel size of a format; zero for Libre. */
	static FIntPoint GetOutputSize(EVibH2OOutputFormat Format);

	/** Applies every setting at once, as if they had all just changed. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Appliquer tout"))
	void ApplyAll();

	/** Reads the current values of the driven actors into the desk, so placing it changes nothing. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Lire l'état actuel de la scène"))
	void CaptureFromScene();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Recalculer le cadrage"))
	void RefreshFraming();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void StartDescent();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void StartArrival();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void StopCamera();

	/** Moves the rail to a position, in Seconds (0 = at once). */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void GlideCameraTo(float RailPosition, float Seconds);

	// -------------------------------------------------------------- Presets

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Préréglages")
	bool SavePreset(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Préréglages")
	bool LoadPreset(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Préréglages")
	bool DeletePreset(const FString& Name);

	UFUNCTION(BlueprintPure, Category = "VibH2O|Préréglages")
	TArray<FString> ListPresets() const;

	/** Folder the presets live in: Saved/VibH2O/Regie of the project. */
	static FString GetPresetFolder();

	// ------------------------------------------------------------- Tablet

	/** The controls the tablet draws, in order. */
	static TConstArrayView<FVibH2ORegieControl> GetControls();

	/** Handles /Regie/... from the tablet or OSC. */
	void HandleRegieMessage(const FVibH2OOscMessage& Message);

	/** Sets one setting by its name, from a number or a #RRGGBB colour. */
	bool SetValueFromText(const FString& Id, const FString& Value);

	/** Adds "regie" to the tablet's state. */
	void WriteRemoteState(const TSharedRef<FJsonObject>& Root) const;

	// ------------------------------------------------------ Framing (pure)

	/**
	 * Whether a camera standing at CameraLocation, looking along Forward,
	 * sees every corner of the room with the margin around it.
	 *
	 * @param HorizontalFOV In degrees.
	 */
	static bool IsFramed(TConstArrayView<FVector> Corners, const FVector& CameraLocation, const FVector& Forward, const FVector& Right,
		const FVector& Up, float HorizontalFOV, float AspectRatio, float Margin);

	// ---------------------------------------------------------------- Actor

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void ResolveConnections();
	void ApplyChanged(bool bForce);
	void ApplyToStage();
	void ApplyToExposure();
	void ApplyToWater();
	void ApplyToCameraController();
	void ApplyToOutput();
	class UCameraComponent* FindRailCamera() const;
	void TickCamera(float DeltaSeconds);
	void TickParticles();
	void GatherParticles();
	float GetRailPosition() const;
	void SetRailPosition(float Position);
	bool ComputeFramedRailPosition(float& OutPosition) const;

	UPROPERTY(Transient)
	FVibH2ORegieSettings Applied;

	/** False until the desk has read the scene once: placing it must not change the look. */
	UPROPERTY()
	bool bCaptured = false;

	/** How much brighter the water glows toward the sun than elsewhere, read from the scene. */
	UPROPERTY()
	float WaterDirectionalRatio = 2.3f;

	TArray<FString> PresetNames;
	void RefreshPresetNames();

	bool bHasApplied = false;
	bool bWaterPending = false;

	// Camera move.
	float MoveFrom = 0.0f;
	float MoveTo = 0.0f;
	float MoveElapsed = 0.0f;
	float MoveDuration = 0.0f;
	float LastWrittenRail = -1.0f;
	double LastFramingTime = 0.0;

	// Particles.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UFXSystemComponent>> Particles;
	int32 ParticlesState = -1;

	FDelegateHandle RegieMessageHandle;
};
