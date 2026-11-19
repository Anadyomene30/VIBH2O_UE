// Tableau 2's arithmetic - the division and the order of the school - as pure
// stateless functions.
//
// Same rule as VibH2OLayoutMath: nothing here knows of actors or worlds, so
// continuity, reversibility and volume conservation are checked by automation
// tests without instantiating anything.
#pragma once

#include "CoreMinimal.h"

/** Where one sister sits, and how big she is, partway through a division. */
struct VIBH2O_API FVibH2ODivisionPose
{
	/** Offset from the family centre - the seat - in the stage's local space, cm. */
	FVector Offset = FVector::ZeroVector;

	/**
	 * Multiplier on the bubble's scale. The mother starts at 1; a daughter
	 * starts at 0, still inside her mother.
	 */
	float ScaleFactor = 1.0f;

	/**
	 * How far this sister has become a body of her own, 0 to 1. Used to fade
	 * her float and drift from her mother's into her own, so the sisters do not
	 * jiggle apart while still nested inside one another.
	 */
	float Individuation = 0.0f;
};

struct VIBH2O_API FVibH2OSchoolMath
{
	/**
	 * Members per family, mother included: 1 + the daughters actually born.
	 *
	 * The ceiling wins over the requested count: past MaxSchoolSize bodies,
	 * fewer daughters are born rather than the frame rate falling over.
	 */
	static int32 ComputeFamilySize(int32 DaughtersPerBubble, int32 OccupiedCount, int32 MaxSchoolSize);

	/** Division progress, 0 to 1, as BlendAlpha crosses [Start, End]. */
	static float ComputeDivisionProgress(float Blend, float Start, float End);

	/**
	 * How far the families have travelled from their seats toward the school.
	 *
	 * Without daughters this IS the blend, untouched. With them, departure waits
	 * until the division is half done: each spectator is first seen to multiply
	 * in place, then the family leaves - while the last of the division is still
	 * settling, so the two gestures overlap instead of queueing mechanically.
	 */
	static float ComputeMigrationBlend(float Blend, float Start, float End, int32 FamilySize);

	/**
	 * Pose of one family member at a given division progress.
	 *
	 * An equal mitosis: at the end every sister has the same size, and the
	 * total volume of the family equals the mother's at EVERY instant, not only
	 * at the ends - the daughters swell inside the mother as she shrinks, then
	 * the sisters part. Each family is turned by its own seed so the room does
	 * not divide in lockstep.
	 *
	 * @param Member     0 for the mother, 1..FamilySize-1 for the daughters.
	 * @param FamilySeed Deterministic seed of the family (the seat index).
	 * @param Spread     Distance from each sister to the family centre once divided, cm.
	 */
	static FVibH2ODivisionPose EvaluateDivision(int32 Member, int32 FamilySize, int32 FamilySeed, float Progress, float Spread);

	/**
	 * Order the school aims for, from the room's collective synchrony: 0 below
	 * DisperseBelow, 1 above VortexAbove, a smooth fade in between.
	 */
	static float ComputeOrderTarget(float Synchrony, float DisperseBelow, float VortexAbove);

	/**
	 * Framerate-independent exponential approach of the order toward its
	 * target. Synchrony is a noisy measurement; unfiltered, it would make the
	 * vortex tremble.
	 */
	static float SmoothOrder(float Current, float Target, float DeltaSeconds, float TimeConstant);

	/** Seed of one family member. The mother keeps her seat's seed unchanged. */
	static int32 MakeMemberSeed(int32 SeatIndex, int32 Member);
};
