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
