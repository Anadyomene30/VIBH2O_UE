// The tableau abstraction.
//
// A tableau does not own bubbles: it is a function that says where each bubble
// goes. The bubbles belong to the stage actor and outlive it. That is what lets
// two drivers run permanently and a single slider interpolate between them -
// a transition that is continuous, reversible, stoppable half way, with no
// state machine anywhere.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VibH2OTypes.h"

#include "VibH2OPositionDriver.generated.h"

class AVibH2OStageActor;

/**
 * Everything a position driver may know about one bubble at one instant.
 *
 * The context is deliberately rich: it must make it possible to write a whole
 * tableau in Blueprint without having to reach back into the subsystem.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OLayoutContext
{
	GENERATED_BODY()

	/** The owning stage actor. It carries the geometry settings. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	TObjectPtr<AVibH2OStageActor> Stage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	FVibH2OSeat Seat;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 Columns = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 Rows = 0;

	/**
	 * Rank among the occupied bubbles, 0-based. Prefer this to the seat index
	 * for any continuous distribution: the room has holes, the spiral does not.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 OccupiedOrdinal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 OccupiedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	FVibH2OIndividualState Individual;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float CollectiveSynchrony = 0.0f;

	/** Time elapsed since the stage actor started, in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float TimeSeconds = 0.0f;

	/** Deterministic seed belonging to this bubble. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 Seed = 0;
};

/**
 * What a driver needs once per frame, before any seat is evaluated.
 *
 * Stateless drivers ignore it. A flock cannot: boids integrate velocities, so
 * the whole school has to step forward once, together, before anyone asks
 * where a single fish is.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2OFrameContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	TObjectPtr<AVibH2OStageActor> Stage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float DeltaSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float TimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 OccupiedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float CollectiveSynchrony = 0.0f;

	/** Average arousal of the room, 0 to 1. A stirred school swims faster. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float AverageExcitation = 0.0f;

	/** How far the morph has moved toward this tableau. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	float BlendAlpha = 0.0f;
};

/**
 * Base class for position drivers. One more tableau is one more class, not a
 * rewrite.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType, Blueprintable, CollapseCategories)
class VIBH2O_API UVibH2OPositionDriver : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Position and orientation of the bubble, in LOCAL space under the stage
	 * actor's root.
	 *
	 * Never in world coordinates: the installation must stay movable, rotatable
	 * and scalable in the level. A world-space computation would work as long
	 * as nothing moved, and break the first time it did.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VibH2O|Tableau")
	FTransform EvaluateSeat(const FVibH2OLayoutContext& Context) const;
	virtual FTransform EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const;

	/**
	 * Called once per frame, before any EvaluateSeat of that frame.
	 *
	 * The default does nothing: a grid needs no preparation. It exists for the
	 * drivers that carry state between frames.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VibH2O|Tableau")
	void PrepareFrame(const FVibH2OFrameContext& Frame);
	virtual void PrepareFrame_Implementation(const FVibH2OFrameContext& Frame);

	/** Name shown in the Details panel and in logs. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VibH2O|Tableau")
	FText GetDisplayName() const;
	virtual FText GetDisplayName_Implementation() const;
};

/**
 * Tableau 1 - the room plan.
 *
 * The geometry settings do not live here but on the stage actor: they must
 * carry the Interp specifier to be animatable in Sequencer, and Sequencer does
 * not animate the properties of an instanced sub-object.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Tableau 1 - Plan de salle"))
class VIBH2O_API UVibH2OGridDriver : public UVibH2OPositionDriver
{
	GENERATED_BODY()

public:
	virtual FTransform EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const override;
	virtual FText GetDisplayName_Implementation() const override;
};

/**
 * Tableau 2 - the flock, first pass.
 *
 * This driver gives the SHAPE: the cone, how it opens up, how it tightens with
 * collective synchrony. That is what makes the transition testable. The real
 * flocking logic - neighbourhood, separation, alignment, cohesion - is out of
 * scope for this delivery, and will replace this computation without anything
 * around it having to change.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Tableau 2 - Vortex"))
class VIBH2O_API UVibH2OVortexDriver : public UVibH2OPositionDriver
{
	GENERATED_BODY()

public:
	virtual FTransform EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const override;
	virtual FText GetDisplayName_Implementation() const override;
};

/**
 * Tableau 2 - the flock, for real.
 *
 * Actual boids: separation, alignment, cohesion, evaluated against neighbours
 * each frame, plus a slow wandering attractor the whole school follows. What
 * makes it read as a school of fish rather than particles in a field is that
 * every bubble turns to face where it is swimming, and that speed is bounded
 * on both sides - a fish never stops dead and never teleports.
 *
 * Collective synchrony is the dial the piece cares about: at 1 the school is
 * tight, aligned and fast; at 0 cohesion and alignment fall away and it
 * scatters into a loose cloud.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Tableau 2 - Banc de poissons"))
class VIBH2O_API UVibH2OFlockDriver : public UVibH2OPositionDriver
{
	GENERATED_BODY()

public:
	virtual void PrepareFrame_Implementation(const FVibH2OFrameContext& Frame) override;
	virtual FTransform EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const override;
	virtual FText GetDisplayName_Implementation() const override;

	/** Radius within which a bubble notices its neighbours, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "10.0"))
	float NeighbourRadius = 320.0f;

	/** Below this distance a bubble actively pushes away, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "1.0"))
	float SeparationRadius = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float SeparationWeight = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float AlignmentWeight = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float CohesionWeight = 1.1f;

	/** Pull toward the wandering attractor that carries the school along. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float AttractorWeight = 1.4f;

	/** Radius of the volume the school stays inside, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "100.0"))
	float BoundsRadius = 700.0f;

	/** Height of the swimming volume, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "100.0"))
	float BoundsHeight = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "1.0"))
	float MinSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "1.0"))
	float MaxSpeed = 420.0f;

	/** How much the room's arousal speeds the school up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float ExcitationSpeedBoost = 0.8f;

	/** How fast the wandering attractor circles, in turns per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc")
	float AttractorSpeed = 0.07f;

	/** Vertical offset of the swimming volume, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc")
	float ZOffset = 200.0f;

private:
	void Reseed(int32 Count, const AVibH2OStageActor* Stage);

	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	bool bSeeded = false;
	float LastTime = -1.0f;
};
