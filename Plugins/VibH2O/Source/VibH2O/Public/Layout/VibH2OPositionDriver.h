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

	/**
	 * 0 for the bubble of the seat itself - the mother - and 1..FamilySize-1
	 * for the daughters born from it on the way to tableau 2. A daughter shares
	 * her mother's seat, individual and heartbeat.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 FamilyMember = 0;

	/** Members per family, mother included. 1 when division is off. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 FamilySize = 1;

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

	/** Members per family, mother included: the school counts OccupiedCount * FamilySize bodies. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O")
	int32 FamilySize = 1;

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
 * One step of the school, as the driver needs it. Plain C++, no reflection:
 * filled from the stage actor by PrepareFrame, or directly by a test.
 */
struct VIBH2O_API FVibH2OSchoolFrame
{
	float DeltaSeconds = 0.0f;
	float TimeSeconds = 0.0f;

	/** Families on stage: one per occupied seat. */
	int32 FamilyCount = 0;

	/** Members per family, mother included. */
	int32 FamilySize = 1;

	/** 0 = dispersed into families, 1 = one vortex. Already smoothed. */
	float Order = 0.0f;

	float AverageExcitation = 0.0f;

	// The cone of the vortex: the stage's own "Vortex" settings, so the shape
	// stays animatable in Sequencer.
	float ConeBaseRadius = 200.0f;
	float ConeTopRadius = 900.0f;
	float ConeHeight = 1200.0f;
	float ConeZOffset = 0.0f;

	/** +1 or -1: which way the vortex turns. */
	float SpinDirection = 1.0f;
};

/**
 * Tableau 2 - the flock, for real.
 *
 * Actual boids: separation, alignment, cohesion, evaluated against neighbours
 * each frame. What makes it read as a school of fish rather than particles in
 * a field is that every bubble turns to face where it is swimming, and that
 * speed is bounded on both sides - a fish never stops dead and never
 * teleports.
 *
 * Collective synchrony is the dial the piece cares about, read as an ORDER
 * between 0 and 1 (two thresholds and a smoothing, set on the stage actor):
 *  - at 1 the whole school mills round a cone that opens upward, like a
 *    tornado of fish, every body swimming the same way round;
 *  - at 0 it breaks into families - a mother and the daughters she divided
 *    into - each a little swarm of its own, the swarms spread evenly through
 *    the volume. Scattered, yet every spectator stays readable.
 * Between the two, both sets of forces are mixed: nothing switches.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Tableau 2 - Banc de poissons"))
class VIBH2O_API UVibH2OFlockDriver : public UVibH2OPositionDriver
{
	GENERATED_BODY()

public:
	virtual void PrepareFrame_Implementation(const FVibH2OFrameContext& Frame) override;
	virtual FTransform EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const override;
	virtual FText GetDisplayName_Implementation() const override;

	/** Steps the whole school once. PrepareFrame calls it; tests call it directly. */
	void StepSchool(const FVibH2OSchoolFrame& School);

	/** The order actually in force, after thresholds and smoothing: 0 dispersed, 1 vortex. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Tableau")
	float GetSchoolOrder() const { return CurrentOrder; }

	int32 GetAgentCount() const { return Positions.Num(); }
	FVector GetAgentPosition(int32 Agent) const { return Positions.IsValidIndex(Agent) ? Positions[Agent] : FVector::ZeroVector; }
	FVector GetAgentVelocity(int32 Agent) const { return Velocities.IsValidIndex(Agent) ? Velocities[Agent] : FVector::ZeroVector; }

	/** Radius within which a bubble notices its neighbours, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "10.0"))
	float NeighbourRadius = 320.0f;

	/**
	 * Below this distance a bubble actively pushes away, in cm. Shrunk
	 * automatically for daughters, which are smaller.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "1.0"))
	float SeparationRadius = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float SeparationWeight = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float AlignmentWeight = 1.5f;

	/**
	 * Pull toward the neighbours, felt only as the school disperses. Faint by
	 * design: the families and the cone already hold it together, and more
	 * makes the dispersed families heap up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float CohesionWeight = 0.06f;

	/**
	 * Pull of each family toward its own place in the volume, when the school
	 * is dispersed. What keeps the families spread evenly - and so readable -
	 * rather than drifting into a heap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float AttractorWeight = 1.4f;

	/** Radius of the volume the dispersed families share, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "100.0"))
	float BoundsRadius = 1100.0f;

	/** Height of that volume, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "100.0"))
	float BoundsHeight = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "1.0"))
	float MinSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "1.0"))
	float MaxSpeed = 420.0f;

	/** How much the room's arousal speeds the school up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc", meta = (ClampMin = "0.0"))
	float ExcitationSpeedBoost = 0.8f;

	/** How fast the centre of the school wanders, in turns per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc")
	float AttractorSpeed = 0.07f;

	/** Vertical offset of the dispersion volume, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc")
	float ZOffset = 200.0f;

	// ----------------------------------------------------- Vortex

	/** How firmly the school swims round the axis once in order, per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Vortex", meta = (ClampMin = "0.0"))
	float SwirlWeight = 1.6f;

	/** Pull back onto the wall of the cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Vortex", meta = (ClampMin = "0.0"))
	float ConeWeight = 2.0f;

	/**
	 * Thickness of the wall of fish, as a fraction of the local radius. Thin
	 * reads as a drawn cone, thick as a living mass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Vortex", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ShellThickness = 0.4f;

	/** Each fish slowly climbs and sinks along the cone; this is how firmly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Vortex", meta = (ClampMin = "0.0"))
	float VerticalWeight = 2.5f;

	/** Pace of that climb and sink, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Vortex", meta = (ClampMin = "0.0"))
	float VerticalCycleSpeed = 0.04f;

	/**
	 * How far the axis of the vortex sways, as a fraction of the volume's
	 * radius. A perfectly still axis reads as a machine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Vortex", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AxisSway = 0.08f;

	// ----------------------------------------------------- Families

	/** How tightly the sisters of a family stay together when dispersed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Familles", meta = (ClampMin = "0.0"))
	float KinWeight = 3.0f;

	/** Speed of the dispersed school relative to the vortex. Calmer reads better. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Familles", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DispersedSpeedFactor = 0.55f;

	/** How fast each family's place drifts through the volume, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banc|Familles", meta = (ClampMin = "0.0"))
	float FamilyDriftSpeed = 0.03f;

private:
	void Reseed(int32 FamilyCount, int32 FamilySize);

	/** Where family F sits in the dispersion volume at time T, before the centre's sway. */
	FVector FamilyHome(int32 Family, int32 FamilyCount, float TimeSeconds) const;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	int32 SeededFamilySize = 1;
	bool bSeeded = false;

	float CurrentOrder = 0.0f;
	bool bOrderPrimed = false;

	// Scratch buffers kept between frames: the school steps every frame, and
	// reallocating them each time would be waste for nothing.
	TArray<FVector> NewVelocities;
	TArray<FVector> FamilyCentres;
	TArray<FVector> FamilyVelocities;
	TArray<int32> CellOrder;
	TArray<uint64> CellKeys;
	TMap<uint64, FIntPoint> CellRanges;
};
