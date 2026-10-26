// The bubble: one actor per seated spectator.
//
// The plugin does not own the look. It pushes named values into a dynamic
// material instance and raises Blueprint events. The mesh, the subsurface
// material, the striations and the rings are artistic work, done in a Blueprint
// subclass of this actor.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VibH2OMood.h"
#include "VibH2OTypes.h"

#include "VibH2OBubbleActor.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OBubbleBeatSignature, int32, BeatCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OBubbleDataSignature, const FVibH2OIndividualState&, State);

/**
 * Names of the parameters pushed to the material.
 *
 * They are configurable so the artist can name their own as they see fit.
 * Clearing a name simply stops that parameter from being sent.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OMaterialParameterNames
{
	GENERATED_BODY()

	// --- The nine contract parameters, described in docs/SPEC.md section F.

	/** 0 to 1, continuous, never jumps. The bedrock of everything periodic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName BeatPhase = TEXT("BeatPhase");

	/** Decaying envelope since the last beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName BeatPulse = TEXT("BeatPulse");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName Bpm = TEXT("Bpm");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName Excitation = TEXT("Excitation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName Synchrony = TEXT("Synchrony");

	/** Striation animation speed, derived from the bubble's real speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName StriationSpeed = TEXT("StriationSpeed");

	/** 1 inside the focus group, less outside it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName FocusMask = TEXT("FocusMask");

	/** 1 when the sensor has gone quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName Staleness = TEXT("Staleness");

	/** Colour taken from the arousal gradient. Vector parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contrat")
	FName TintColor = TEXT("TintColor");

	// --- Beyond the contract. Clear the name to stop sending them.

	/** Collective synchrony of the room. Useful to tableau 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supplément")
	FName CollectiveSynchrony = TEXT("CollectiveSynchrony");

	/** Position of the morph between the two tableaux. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supplément")
	FName BlendAlpha = TEXT("BlendAlpha");

	// --- The room's mood, and synchrony. See FVibH2OBubbleMood.

	/** Weight of the calm mood, 0 to 1. The three weights sum to one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName MoodCalm = TEXT("MoodCalm");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName MoodNormal = TEXT("MoodNormal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName MoodExcited = TEXT("MoodExcited");

	/** Light of the striations, blended across moods. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName Glow = TEXT("MoodGlow");

	/** Restlessness of the surface, blended across moods. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName Turbulence = TEXT("MoodTurbulence");

	/**
	 * This bubble's own striation phase, 0 to 1. Accumulated, never
	 * time * speed: a change of mood changes how fast it runs, not where it is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName StripePhase = TEXT("StripePhase");

	/** The phase shared by the whole room, 0 to 1. What synchrony aligns on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName RoomStripePhase = TEXT("RoomStripePhase");

	/** How much this bubble falls in step with the room, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName Coherence = TEXT("Coherence");

	/** Strength of the synchrony halo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName SyncHalo = TEXT("SyncHalo");

	/** The room's smoothed mean arousal, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName RoomExcitation = TEXT("RoomExcitation");

	/** Overall light of the bubbles, set on the stage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName Brightness = TEXT("BubbleBrightness");

	/**
	 * A stable random value per bubble, 0 to 1, drawn from its seat. Unlike
	 * one drawn from the object's position, it does not drift as the bubble
	 * floats.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambiance")
	FName BubbleSeed = TEXT("BubbleSeed");
};

/**
 * What the room's mood and synchrony mean for one bubble, this frame.
 *
 * Filled by the stage, readable from the bubble Blueprint, and pushed to the
 * material under the names above.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OBubbleMood
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	FVibH2OMoodWeights Weights;

	/** The mood the room is in, or heading for. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	EVibH2OMood Mood = EVibH2OMood::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float Glow = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float Turbulence = 1.0f;

	/** Bands per second this bubble's striations run at. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float StripeSpeed = 0.3f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float RoomStripePhase = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float Coherence = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float SyncHalo = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float RoomExcitation = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Ambiance")
	float Brightness = 1.0f;
};

/**
 * A bubble.
 *
 * It belongs to no tableau: the stage actor owns it, permanently. A tableau
 * only says where it goes. Its beat phase does not even live here but in the
 * subsystem, keyed by individual - which is how it survives any room rebuild.
 */
UCLASS(Blueprintable, BlueprintType, HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance), PrioritizeCategories = ("VibH2O"), meta = (DisplayName = "VibH2O — Bulle"))
class VIBH2O_API AVibH2OBubbleActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OBubbleActor();

	virtual void BeginPlay() override;

#if WITH_EDITOR
	/**
	 * Live bubbles in the editor world are not selectable: they are rebuilt
	 * from data at any moment, so a click must not grab one - and moving one
	 * by hand would be overwritten the next frame anyway.
	 */
	virtual bool IsSelectable() const override;
#endif

	// ------------------------------------------------------------- Identity

	/** OSC index of the occupied seat, 1-based. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	int32 SeatIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	int32 IndividualId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	int32 Column = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	int32 Row = 0;

	/**
	 * 0 for the bubble of the seat itself, 1 and up for the daughters it
	 * divides into on the way to tableau 2. A daughter shares her mother's
	 * seat, individual and heartbeat, so she raises the same events: test this
	 * in a Blueprint to play a sound once per spectator rather than once per
	 * sister.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	int32 FamilyMember = 0;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Bulle")
	bool IsDaughter() const { return FamilyMember > 0; }

	/** Last known state of the individual. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	FVibH2OIndividualState State;

	/** Local speed measured last frame, in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	float MeasuredSpeed = 0.0f;

	/** Focus mask value, 1 inside the group. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	float FocusMask = 1.0f;

	/** The room's mood and synchrony, as they apply to this bubble. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	FVibH2OBubbleMood Mood;

	/** This bubble's striation phase, 0 to 1, accumulated frame by frame. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	float StripePhase = 0.0f;

	// ------------------------------------------------------------- Material

	/**
	 * Component carrying the material to drive. Left empty, the first mesh
	 * component found is used - which is enough for most Blueprints.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Materiau", meta = (DisplayName = "Mesh piloté (vide = le premier trouvé)"))
	TObjectPtr<UMeshComponent> TargetMeshComponent;

	/** Material slot to instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Materiau", meta = (DisplayName = "Emplacement de matériau", ClampMin = "0", UIMax = "8"))
	int32 MaterialSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Materiau", AdvancedDisplay, meta = (DisplayName = "Noms des paramètres du matériau"))
	FVibH2OMaterialParameterNames ParameterNames;

	/** Creates the dynamic instance, or returns the one already created. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Materiau")
	UMaterialInstanceDynamic* GetOrCreateMaterial();

	/** Changes the target component and recreates the dynamic instance. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Materiau")
	void SetMaterialTarget(UMeshComponent* InMesh, int32 InSlot = 0);

	UFUNCTION(BlueprintPure, Category = "VibH2O|Materiau")
	UMaterialInstanceDynamic* GetDynamicMaterial() const { return DynamicMaterial; }

	// --------------------------------------------------------------- Events

	/** One beat of this person's heart. */
	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OBubbleBeatSignature OnBeat;

	/** This person's data has just been refreshed. */
	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OBubbleDataSignature OnDataUpdated;

	/** Override in the bubble Blueprint: sound, Niagara, particles. */
	UFUNCTION(BlueprintImplementableEvent, Category = "VibH2O|Evenements", meta = (DisplayName = "Sur battement"))
	void ReceiveBeat(int32 BeatCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "VibH2O|Evenements", meta = (DisplayName = "Sur mise a jour des donnees"))
	void ReceiveDataUpdated(const FVibH2OIndividualState& NewState);

	/** Called when the bubble is assigned to a seat, or reassigned. */
	UFUNCTION(BlueprintImplementableEvent, Category = "VibH2O|Evenements", meta = (DisplayName = "Sur affectation a un siege"))
	void ReceiveSeatAssigned(int32 InSeatIndex, int32 InIndividualId);

	// -------------------------------------------------------------- Internal

	/** Called by the stage actor on creation or reassignment. */
	void AssignSeat(const FVibH2OSeat& Seat);

	/**
	 * Called by the stage actor every frame, before UpdateFromState: what the
	 * room's mood means for this bubble. Advances the striation phase.
	 */
	void SetMood(const FVibH2OBubbleMood& InMood, float DeltaTime);

	/** Called by the stage actor on a daughter: her seat is her mother's. */
	void AssignDaughter(const FVibH2OSeat& Seat, int32 InFamilyMember);

	/** Shows or hides the number written on this bubble. */
	void SetSeatLabelVisible(bool bVisible);

	/** What the number says, how tall it is (share of the diameter), its colour. */
	void SetSeatLabelStyle(EVibH2OSeatNumber Source, float HeightFraction, const FLinearColor& Color);

	/**
	 * Writes the number over the middle of the bubble, as seen from the viewer.
	 *
	 * The bubble is opaque, so the text cannot sit at its true centre: it sits
	 * on the surface facing the viewer, which reads as the centre from there.
	 * A text component renders on a fixed plane, so it also turns to face the
	 * viewer. The stage actor passes the view position once per frame rather
	 * than every bubble asking for it.
	 */
	void PlaceSeatLabel(const FVector& ViewLocation);

	UTextRenderComponent* GetSeatLabel() const { return SeatLabel; }

	/** Centre and radius of the bubble as drawn this frame, in world space. */
	void GetDrawnSphere(FVector& OutCentre, float& OutRadius) const;

	/**
	 * Called by the stage actor every frame, once the bubble has been placed.
	 * Updates the state, measures the speed, pushes the material parameters and
	 * raises the events.
	 */
	void UpdateFromState(const FVibH2OIndividualState& NewState, float InFocusMask, const FLinearColor& Tint,
		float CollectiveSynchrony, float BlendAlpha, float StriationScale, float DeltaTime);

private:
	void PushMaterialParameters(const FLinearColor& Tint, float CollectiveSynchrony, float BlendAlpha, float StriationScale);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O|Bulle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> BubbleRoot;

	/**
	 * Demonstration mesh. It proves the data flows; it claims no artistic
	 * direction whatsoever. A Blueprint subclass replaces or hides it.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O|Bulle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DemoMesh;

	/**
	 * The spectator's number, over the middle of the bubble.
	 *
	 * Lets each spectator find themselves in the room - and is how a seating
	 * plan is checked against the real room, a wrong seat order spotted in
	 * one look.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O|Bulle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> SeatLabel;

	EVibH2OSeatNumber SeatLabelSource = EVibH2OSeatNumber::Individual;
	float SeatLabelHeightFraction = 0.55f;
	/** -1 until a text has been written: forces the first write. */
	int32 SeatLabelShownNumber = -1;

	/** Local position last frame, used to measure speed. */
	FVector PreviousLocalLocation = FVector::ZeroVector;
	bool bHasPreviousLocation = false;

	int32 LastBeatCount = 0;

	/** Stable per-bubble random value, from the seat index. */
	float Seed = 0.0f;
};
