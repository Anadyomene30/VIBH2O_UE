// Tulles in depth: one picture per tulle, at the room's real proportions.
//
// The show's room has several tulles one behind the other: the face tulle,
// which the rail camera fills, and further back the cour and jardin tulles
// (and the bac, a band on the floor in front). Each is a window onto the same
// virtual world. This rig places those windows in the world, as the main
// camera sees it, with the ratios measured in the hall, and renders each
// back window from the same eye, with its own off-axis frustum, into its own
// render target - and, when the SpoutUE plugin is present, its own Spout
// sender, so Resolume receives one feed per projector.
//
// Its scale comes from the camera: the face tulle is the cross-section of the
// camera's view at a chosen distance, so a real metre is worth that width
// divided by eight. Every other tulle follows from its offset to the face
// tulle, measured in the hall in metres. The virtual world itself keeps its
// own size: only the tulles' relations to one another have to be true.
//
// Depth is split by tulle: a bubble lying behind a back tulle's depth and
// inside that tulle's frame, as the eye sees it, is shown there and hidden
// from the main camera; every other bubble stays on the face tulle. The relief
// is then real for every seat, since the pictures hang at real distances.
//
// Off by default: until it is switched on the rig only draws its guides in the
// editor and touches neither the main camera nor the bubbles.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"

#include "VibH2OTulleRig.generated.h"

class AVibH2OStageActor;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** How a tulle hangs in the hall. */
UENUM(BlueprintType)
enum class EVibH2OTulleFacing : uint8
{
	Standing UMETA(DisplayName = "Debout, face au public"),
	Floor UMETA(DisplayName = "Au sol"),
};

/**
 * One tulle as measured in the hall, in metres, relative to the face tulle:
 * the origin is the foot of the face tulle, in its middle.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OTulle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Nom"))
	FString Name = TEXT("Tulle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Utilisé"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Position"))
	EVibH2OTulleFacing Facing = EVibH2OTulleFacing::Standing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Largeur (m)", ClampMin = "0.1", ToolTip = "Largeur de l'image projetée, vue du public."))
	float Width = 2.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Hauteur (m)", ClampMin = "0.1", ToolTip = "Debout : hauteur de l'image. Au sol : sa profondeur, du bord côté public au bord côté fond."))
	float Height = 5.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Décalage latéral (m)", ToolTip = "Du milieu du tulle face au milieu de celui-ci. Positif vers cour (à droite du public), négatif vers jardin."))
	float Lateral = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Profondeur (m)", ToolTip = "Distance derrière le tulle face, en s'éloignant du public. Négatif : devant. Au sol : celle du milieu de la bande."))
	float Depth = 2.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Hauteur du bas (m)", ToolTip = "Debout : hauteur du bord bas de l'image au-dessus du plateau. Au sol : hauteur du sol (0 = plateau)."))
	float Elevation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Résolution (px)", ToolTip = "Celle de la mire du tulle."))
	FIntPoint Resolution = FIntPoint(960, 1920);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Bulles : à partir de (m)", ToolTip = "Ce tulle prend les bulles plus profondes que ceci, mesuré derrière le tulle face, et qui tombent dans son cadre."))
	float DepthFrom = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Bulles : jusqu'à (m)"))
	float DepthTo = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tulle", meta = (DisplayName = "Nom Spout"))
	FString SpoutName = TEXT("VibH2O Tulle");
};

/** The face tulle in the world: its foot, its axes, and what a real metre is worth there. */
struct VIBH2O_API FVibH2OTulleFrame
{
	/** Foot of the face tulle, in its middle. */
	FVector Origin = FVector::ZeroVector;
	/** Toward cour, the audience's right. */
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;
	/** Away from the audience, behind the face tulle. */
	FVector Back = FVector::ForwardVector;
	/** Virtual centimetres per real metre. */
	double Scale = 100.0;
};

/** An eye looking through a rectangle: the off-axis frustum of one tulle. */
struct VIBH2O_API FVibH2OOffAxisView
{
	FVector Eye = FVector::ZeroVector;
	/** Perpendicular to the tulle, away from the eye. */
	FVector Forward = FVector::ForwardVector;
	/** Along the tulle's bottom edge, left to right as seen by the eye. */
	FVector Right = FVector::RightVector;
	/** Along the tulle's left edge, bottom to top as seen by the eye. */
	FVector Up = FVector::UpVector;
	/** From the eye to the tulle's plane. */
	double Distance = 0.0;
	/** Edges of the tulle in the eye's frame, at Distance. */
	double Left = 0.0;
	double RightEdge = 0.0;
	double Bottom = 0.0;
	double Top = 0.0;

	/** Whether a point falls inside the tulle, as the eye sees it, and in front of the eye. */
	bool Contains(const FVector& Point) const;

	/** Reversed-Z, infinite far plane, as the renderer expects. */
	FMatrix MakeProjectionMatrix(double NearPlane) const;

	/** A symmetric horizontal field of view wide enough to hold the whole frustum, for culling. */
	float GetEnclosingFOV(double AspectRatio) const;
};

UCLASS(meta = (DisplayName = "VibH2O — Tulles en profondeur"),
	HideCategories = (Actor, Rendering, Replication, Collision, HLOD, Input, LOD, Cooking, Physics, Networking, DataLayers, LevelInstance, WorldPartition),
	PrioritizeCategories = ("VibH2O"))
class VIBH2O_API AVibH2OTulleRig : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OTulleRig();

	/**
	 * Off: the rig only draws its guides in the editor. On, during play: each
	 * tulle used renders its picture and the main camera loses the bubbles it
	 * hands over.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tulles", meta = (DisplayName = "Actif"))
	bool bActive = false;

	/** The face tulle, as measured: the rail camera fills it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tulles", meta = (DisplayName = "Tulle face : largeur (m)", ClampMin = "0.1"))
	float FaceWidth = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tulles", meta = (DisplayName = "Tulle face : hauteur (m)", ClampMin = "0.1"))
	float FaceHeight = 6.0f;

	/**
	 * How far in front of the camera the face tulle stands in the virtual
	 * world, in cm. It sets the scale: the further, the larger a real metre.
	 * Zero: the stage's plane, so the room plan lies on the face tulle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Tulles", meta = (DisplayName = "Distance du tulle face (cm, 0 = la scène)", ClampMin = "0.0"))
	float FacePlaneDistance = 0.0f;

	/** The other tulles, measured from the face tulle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tulles", meta = (DisplayName = "Autres tulles", TitleProperty = "Name"))
	TArray<FVibH2OTulle> Tulles;

	/** Keeps a bubble on its tulle until it is this far past the edge of the band, in metres: no flicker at the boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tulles", meta = (DisplayName = "Hystérésis (m)", ClampMin = "0.0", ClampMax = "2.0"))
	float Hysteresis = 0.1f;

	/** Water and atmosphere off on the back tulles: what they do not light stays black, so transparent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tulles", meta = (DisplayName = "Tulles du fond : fond noir"))
	bool bBlackBackground = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Tulles", meta = (DisplayName = "Dessiner les tulles dans l'éditeur"))
	bool bShowGuides = true;

	// ---------------------------------------------------------- Connections

	/** Empty: the first stage of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Scène (vide = la première)"))
	TObjectPtr<AVibH2OStageActor> Stage;

	/** Empty: during play, the camera the player looks through; in the editor, the first camera hanging from a rail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Caméra (vide = la caméra active)"))
	TObjectPtr<AActor> Camera;

	// ------------------------------------------------------------- Readouts

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Échelle (cm virtuels par mètre réel)"))
	float VirtualCmPerMetre = 0.0f;

	/** Where a spectator sees the face tulle under the camera's angle: from there the picture is geometrically exact. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Spectateur de référence : distance au tulle face (m)"))
	float ReferenceSeatDistance = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Bulles sur le tulle face"))
	int32 BubblesOnFace = 0;

	/** Bubbles each tulle shows, in the order of the list. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Bulles par tulle"))
	TArray<int32> BubblesPerTulle;

	// ---------------------------------------------------------------- Actions

	/** Puts back the room measured on 4 September 2026: jardin, cour, and the bac on the floor (unused). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "VibH2O", meta = (DisplayName = "Tulles de la salle mesurée"))
	void ResetToMeasuredRoom();

	/** Works out the frame, the views and the share of bubbles; done every tick, callable for a test. */
	void UpdateLayout(float DeltaSeconds);

	/** The picture of a tulle, while active; null otherwise. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tulles")
	UTextureRenderTarget2D* GetTulleTarget(int32 TulleIndex) const;

	/** The tulle a bubble is shown on; INDEX_NONE for the face tulle. */
	int32 GetTulleOfActor(const AActor* Actor) const;

	// ------------------------------------------------------ Geometry (pure)

	/**
	 * The face tulle as the cross-section of a view at Distance: its width
	 * fills the horizontal field, its height follows the real ratio.
	 *
	 * @param HorizontalFOV In degrees.
	 */
	static bool ComputeFaceFrame(const FVector& Eye, const FRotator& ViewRotation, float HorizontalFOV, double Distance,
		float InFaceWidth, float InFaceHeight, FVibH2OTulleFrame& OutFrame);

	/** The three corners of a tulle in the world, as seen from the audience. */
	static void GetTulleCorners(const FVibH2OTulleFrame& InFrame, const FVibH2OTulle& Tulle,
		FVector& OutBottomLeft, FVector& OutBottomRight, FVector& OutTopLeft);

	/** False when the eye stands on or behind the tulle's plane. */
	static bool ComputeOffAxisView(const FVector& Eye, const FVector& BottomLeft, const FVector& BottomRight, const FVector& TopLeft,
		FVibH2OOffAxisView& OutView);

	/** Metres behind the face tulle. */
	static double GetRealDepth(const FVibH2OTulleFrame& InFrame, const FVector& Point);

	/**
	 * Which tulle shows a point: the first used whose depth band holds it and
	 * whose frame it falls in; INDEX_NONE for the face tulle. The tulle it was
	 * on keeps it while it stays within Hysteresis of the band.
	 *
	 * @param bViewValid One per tulle: false where the eye cannot see the tulle.
	 */
	static int32 PickTulle(TConstArrayView<FVibH2OTulle> InTulles, TConstArrayView<FVibH2OOffAxisView> InViews, TConstArrayView<bool> bViewValid,
		const FVibH2OTulleFrame& InFrame, const FVector& Point, int32 CurrentTulle, float InHysteresis);

	// ---------------------------------------------------------------- Actor

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	static TArray<FVibH2OTulle> MakeMeasuredRoom();
	void ResolveConnections();
	bool GetEyeView(float DeltaSeconds, FMinimalViewInfo& OutView) const;
	void GatherBubbles(TArray<AActor*>& OutActors) const;
	bool ShouldRender() const;
	void BuildOutputs();
	void ReleaseOutputs();
	void ApplyOutputs();
	void ApplyHiddenActors(bool bRelease);
	uint32 ComputeOutputsSignature() const;
	void DrawGuides() const;

	/** One output per tulle of the list; null for a tulle unused when they were built. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneCaptureComponent2D>> Captures;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> Targets;

	/** SpoutUE senders, created by name so the plugin stays optional. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> Senders;

	/** The actors this rig hid from the main camera, so it hands back exactly those. */
	TArray<TWeakObjectPtr<AActor>> HiddenFromMain;

	TMap<TWeakObjectPtr<AActor>, int32> Assignment;

	/** The eye of the last layout: the main camera's position, lens and post process. */
	FMinimalViewInfo EyeView;
	FVibH2OTulleFrame Frame;
	TArray<FVibH2OOffAxisView> Views;
	TArray<bool> ViewValid;
	bool bFrameValid = false;
	bool bOutputsBuilt = false;
	/** Signature of the tulles the outputs were built for: rebuilt when it changes. */
	uint32 OutputsSignature = 0;
};
