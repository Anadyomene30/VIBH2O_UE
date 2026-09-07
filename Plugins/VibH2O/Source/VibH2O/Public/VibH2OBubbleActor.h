// The bubble: one actor per seated spectator.
//
// The plugin does not own the look. It pushes named values into a dynamic
// material instance and raises Blueprint events. The mesh, the subsurface
// material, the striations and the rings are artistic work, done in a Blueprint
// subclass of this actor.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supplement")
	FName CollectiveSynchrony = TEXT("CollectiveSynchrony");

	/** Position of the morph between the two tableaux. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supplement")
	FName BlendAlpha = TEXT("BlendAlpha");
};

/**
 * A bubble.
 *
 * It belongs to no tableau: the stage actor owns it, permanently. A tableau
 * only says where it goes. Its beat phase does not even live here but in the
 * subsystem, keyed by individual - which is how it survives any room rebuild.
 */
UCLASS(Blueprintable, BlueprintType)
class VIBH2O_API AVibH2OBubbleActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OBubbleActor();

	virtual void BeginPlay() override;

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

	/** Last known state of the individual. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	FVibH2OIndividualState State;

	/** Local speed measured last frame, in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	float MeasuredSpeed = 0.0f;

	/** Focus mask value, 1 inside the group. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Bulle")
	float FocusMask = 1.0f;

	// ------------------------------------------------------------- Material

	/**
	 * Component carrying the material to drive. Left empty, the first mesh
	 * component found is used - which is enough for most Blueprints.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Materiau")
	TObjectPtr<UMeshComponent> TargetMeshComponent;

	/** Material slot to instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Materiau", meta = (ClampMin = "0"))
	int32 MaterialSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Materiau")
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

	/** Shows or hides the number floating over this bubble. */
	void SetSeatLabelVisible(bool bVisible);

	/**
	 * Turns the number toward the viewer.
	 *
	 * A text component renders on a fixed plane, so without this it is edge-on
	 * and invisible from most angles. The stage actor passes the view position
	 * once per frame rather than every bubble asking for it.
	 */
	void OrientSeatLabel(const FVector& ViewLocation);

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
	 * The individual's number, floating over the bubble.
	 *
	 * Not decoration: it is how a seating plan is checked against the real
	 * room, and how a wrong seat order is spotted in one look.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VibH2O|Bulle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> SeatLabel;

	/** Local position last frame, used to measure speed. */
	FVector PreviousLocalLocation = FVector::ZeroVector;
	bool bHasPreviousLocation = false;

	int32 LastBeatCount = 0;
};
