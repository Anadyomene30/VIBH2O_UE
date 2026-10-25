// The math behind hand-steered attractors and exclusion zones.
//
// Plain structs and static functions, no UObject: everything that decides
// whether a fade is smooth or a bubble stays out of a performer's way is here,
// where a test can reach it without spawning a world.
#pragma once

#include "CoreMinimal.h"

/**
 * A critically damped follower: the shape every fade in this file takes.
 *
 * Why a spring rather than a ramp. A ramp eased by a smoothstep looks right
 * until someone changes their mind half way: the velocity then flips sign in
 * one frame, and two hundred bubbles visibly bounce. A critically damped spring
 * starts with zero velocity, lands with zero velocity, never overshoots, and
 * when its target is reversed mid-flight it decelerates and turns back - the
 * velocity stays continuous whatever the operator does on the iPad.
 *
 * SettleSeconds is the time to cover 99% of the distance, which is what a
 * person setting a "4 s fade" means.
 */
struct VIBH2O_API FVibH2OSpring
{
	float Value = 0.0f;
	float Velocity = 0.0f;

	/** Advances toward Target. Exact integration: stable at any frame rate. */
	void Step(float Target, float SettleSeconds, float DeltaSeconds);

	/** Jumps straight to a value, at rest. */
	void Reset(float InValue)
	{
		Value = InValue;
		Velocity = 0.0f;
	}
};

/** The 2D shape of an exclusion zone, seen from the camera. */
enum class EVibH2OZoneShapeKind : uint8
{
	Ellipse,
	Rectangle
};

/** Parameters of one exclusion push, in the zone's 2D frame, in cm. */
struct VIBH2O_API FVibH2OZonePushParams
{
	EVibH2OZoneShapeKind Shape = EVibH2OZoneShapeKind::Ellipse;

	/** Half width and half height of the zone. */
	FVector2D HalfSize = FVector2D(100.0, 100.0);

	/**
	 * Soft band outside the shape, in cm. A bubble entering it is eased toward
	 * the edge rather than stopped dead on it.
	 */
	float Margin = 60.0f;

	/**
	 * How fast a bubble trapped inside may swing round to the other side, in
	 * radians per second. This is what makes a bubble whose path crosses the
	 * zone glide round its edge instead of jumping across it.
	 */
	float GlideRadiansPerSecond = 2.6f;

	/** Zone activation, 0 to 1. */
	float Weight = 1.0f;
};

/**
 * What an exclusion zone remembers about one bubble: the side it is being
 * pushed out of. Without this memory the push could not be continuous - see
 * FVibH2OSteeringMath::PushOutOfZone.
 */
struct VIBH2O_API FVibH2OZoneBubbleState
{
	FVector2D Direction = FVector2D(1.0, 0.0);
	bool bHasDirection = false;
};

struct VIBH2O_API FVibH2OSteeringMath
{
	// ------------------------------------------------------------ Selection

	/**
	 * Stable position of a bubble on the selection circle, in [0, 1).
	 *
	 * The same for every attractor, on purpose: two attractors with adjacent
	 * ranges - start 0 share 0.5, start 0.5 share 0.5 - then split the room
	 * exactly in two, with no overlap and nobody left out.
	 */
	static float SelectionHash(int32 Seed);

	/** True when the hash falls inside [Start, Start + Share), wrapping round. */
	static bool IsInShare(float Hash, float Start, float Share);

	/**
	 * Fade duration of one bubble. Spreading the durations round the nominal
	 * one makes the room leave as a flow rather than as one rigid block, while
	 * keeping the average where the operator set it.
	 *
	 * @param Stagger 0 for everyone together, 1 for durations spread from
	 *                half to one and a half times the nominal one.
	 */
	static float StaggeredDuration(float Nominal, int32 Seed, float Stagger);

	// ---------------------------------------------------------------- Cloud

	/**
	 * Where a bubble sits inside an attractor's cloud, in the unit ball.
	 *
	 * Uniform in volume - the radius goes as the cube root - so the cloud is as
	 * dense at its heart as at its rim, and it breathes: each bubble's radius
	 * swells and settles with a phase of its own.
	 */
	static FVector CloudPoint(int32 Seed, float TimeSeconds, float BreathAmount, float BreathSpeed);

	/**
	 * Combines the pull of every attractor on one bubble.
	 *
	 * Below a total weight of 1 the bubble sits between its base position and
	 * the weighted targets; above it, between the targets only, in proportion.
	 * The two rules agree exactly at 1, so stacking attractors never jumps.
	 */
	static FVector BlendTowardTargets(const FVector& Base, TArrayView<const FVector> Targets, TArrayView<const float> Weights);

	// ------------------------------------------------------------ Exclusion

	/**
	 * Norm of a point against the zone's shape: below 1 inside, 1 on the edge.
	 * An ellipse is the 2-norm, a rectangle a 6-norm - a box with rounded
	 * corners, whose edge has no corner for a bubble to snag on.
	 */
	static float ShapeNorm(const FVector2D& Point, const FVector2D& HalfSize, EVibH2OZoneShapeKind Shape);

	/**
	 * Eases a signed distance out of a soft band. Identity beyond Margin;
	 * inside it, and inside the shape, the result stays strictly positive and
	 * shrinks toward 0 - never negative, so never inside. Continuous with a
	 * continuous slope at Margin.
	 */
	static float SoftOutside(float SignedDistance, float Margin);

	/**
	 * Pushes a point out of a zone, in the zone's 2D frame.
	 *
	 * The push cannot be a pure function of the position. Any map that sends
	 * the inside of a disc onto its rim must tear somewhere: a bubble whose path
	 * crosses the middle of the zone would jump from one side to the other in a
	 * single frame - in front of the performer, which is the worst place. So
	 * each bubble keeps the side it is pushed toward, and that side may only
	 * swing round at GlideRadiansPerSecond: the bubble slides round the edge to
	 * its new side instead.
	 *
	 * Outside the soft band nothing moves, and the remembered side quietly
	 * follows the real one, so the next entry starts aligned.
	 */
	static FVector2D PushOutOfZone(const FVector2D& Point, const FVibH2OZonePushParams& Params,
		FVibH2OZoneBubbleState& State, int32 Seed, float DeltaSeconds);
};
