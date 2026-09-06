// All of the plugin's geometry, as pure stateless functions.
//
// This file knows nothing of actors, components or worlds. That is deliberate:
// curvature, drift and vortex are thereby verifiable by automation tests,
// without instantiating anything at all.
#pragma once

#include "CoreMinimal.h"
#include "VibH2OTypes.h"

/** Position and orientation of a bubble, in local space under the stage root. */
struct VIBH2O_API FVibH2OSeatLayout
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * Deterministic value noise, written here rather than borrowed from FMath.
 *
 * Two reasons. First: the result must be identical from one launch to the next,
 * which an engine implementation does not contractually guarantee. Second: the
 * relief of the room must stay the same between 5.5 and 5.8, otherwise the room
 * would shift merely by changing engine version.
 */
struct VIBH2O_API FVibH2ONoise
{
	/** Deterministic bit mixing. */
	static uint32 Hash(uint32 X);
	static uint32 Hash2(int32 X, int32 Y);

	/** Deterministic float in [0,1] from an integer seed. */
	static float UnitFloat(int32 Seed);

	/** Deterministic float in [-1,1]. */
	static float SignedFloat(int32 Seed);

	/**
	 * 2D value noise in [-1,1], continuous and smooth.
	 *
	 * Pure per-seat randomness would decorrelate neighbours and make the rows
	 * unreadable; this noise makes the sheet undulate while keeping them
	 * legible.
	 */
	static float Value2D(float X, float Y);
};

/** Parameters of the tableau 1 grid. Plain struct, no reflection. */
struct VIBH2O_API FVibH2OGridParams
{
	int32 Columns = 1;
	int32 Rows = 1;

	/** Lateral gap between two neighbouring seats of a row, in cm. */
	float ColumnSpacing = 120.0f;

	/** Depth gap between two rows, in cm. */
	float RowSpacing = 140.0f;

	/**
	 * SIGNED angle per column step, in degrees. Zero gives an exactly straight
	 * room; positive and negative curve it either way.
	 *
	 * The angle is set per seat rather than as a radius because a straight room
	 * would correspond to an infinite radius, impossible to type into a field.
	 */
	float CurvatureAngleDeg = 0.0f;

	/**
	 * True: constant angle per seat, so the back rows are wider - the geometry
	 * of a real theatre. False: constant lateral gap at every row.
	 */
	bool bFanOut = true;

	/** Bubbles rotate to face the centre of curvature. */
	bool bOrientToCenter = true;

	/** Extra rotation applied to every bubble, in degrees. */
	FRotator OrientationOffset = FRotator::ZeroRotator;

	/** Vertical rise per row, in cm. Reproduces raked seating. */
	float ElevationPerRow = 0.0f;

	/**
	 * Shape of the rise, evaluated over [0,1] from the first to the last row.
	 * Null means a linear rake, in which case Z is exactly
	 * Row * ElevationPerRow.
	 */
	const FRichCurve* ElevationCurve = nullptr;

	/** Amplitude of the noise-driven depth offset, in cm. */
	float DepthAmplitude = 0.0f;

	/** Noise scale. Small means broad swells, large means tight relief. */
	float DepthNoiseScale = 0.25f;

	/** Common height of the whole room, in cm. */
	float BaseHeight = 0.0f;
};

/** Parameters of the tableau 2 vortex. */
struct VIBH2O_API FVibH2OVortexParams
{
	/** Total height of the cone, in cm. */
	float Height = 1200.0f;

	/** Radius at the base of the cone. */
	float BaseRadius = 200.0f;

	/** Radius at the top. Larger than BaseRadius for a cone that opens up. */
	float TopRadius = 900.0f;

	/** Number of spiral turns over the whole height. */
	float Turns = 3.0f;

	/** Overall rotation speed, in turns per second. */
	float SpinSpeed = 0.05f;

	/** Maximum angular scatter when the audience drifts apart, in turns. */
	float AngleDispersion = 0.35f;

	/** Maximum radial scatter, as a fraction of the local radius. */
	float RadiusDispersion = 0.45f;

	/** Maximum vertical scatter, as a fraction of the height. */
	float HeightDispersion = 0.12f;

	/**
	 * Share of individual synchrony in the scatter computation. 0 means
	 * collective synchrony alone sets the order, as the piece intends. 1 means
	 * everyone scatters according to their own synchrony.
	 */
	float IndividualInfluence = 0.0f;

	/** Vertical offset of the whole cone, in cm. */
	float ZOffset = 0.0f;
};

/** Parameters of the permanent float. */
struct VIBH2O_API FVibH2OFloatParams
{
	float Amplitude = 12.0f;
	float Speed = 0.35f;
	float RotationAmount = 6.0f;
};

/** Parameters of the arousal-driven drift. */
struct VIBH2O_API FVibH2ODriftParams
{
	/** Below this threshold, complete stillness. */
	float Threshold = 0.4f;

	/** Drift amplitude at full arousal, in cm. */
	float Intensity = 45.0f;

	/** Slow rotation speed of the drift direction. */
	float DirectionSpeed = 0.12f;

	/**
	 * Fraction of the seat spacing the drift may not exceed. Below 0.5 a bubble
	 * mathematically cannot reach the neighbouring cell.
	 */
	float MaxRatio = 0.35f;
};

/** Layout functions. */
struct VIBH2O_API FVibH2OLayoutMath
{
	// ------------------------------------------------------- Seat index

	/**
	 * Converts a 1-based OSC index into 0-based column and row.
	 *
	 * This is where pitfall 4 of the protocol plays out. A square grid hides
	 * the mistake entirely; only a non-square room reveals it.
	 *
	 * @return false when the index falls outside the grid.
	 */
	static bool SeatIndexToColumnRow(int32 SeatIndex, int32 Columns, int32 Rows, EVibH2OSeatOrder Order, int32& OutColumn, int32& OutRow);

	/** The inverse, 1-based. Returns 0 when the coordinates are off-grid. */
	static int32 ColumnRowToSeatIndex(int32 Column, int32 Row, int32 Columns, int32 Rows, EVibH2OSeatOrder Order);

	// -------------------------------------------------------- Tableau 1

	/**
	 * Position and orientation of a seat in the grid, in local space.
	 *
	 * Guaranteed property: at exactly zero CurvatureAngleDeg the result is
	 * exactly the straight grid - not an approximation within epsilon. Sweeping
	 * the curvature from negative to positive therefore passes through a
	 * perfectly straight room.
	 */
	static FVibH2OSeatLayout EvaluateGrid(const FVibH2OGridParams& Params, int32 Column, int32 Row);

	// -------------------------------------------------------- Tableau 2

	/**
	 * Position of a bubble in the vortex.
	 *
	 * @param Ordinal Rank of the bubble among the occupied ones, 0-based. This
	 *        is not the seat index: using the seat index would leave holes in
	 *        the spiral wherever the room has an empty seat.
	 */
	static FVibH2OSeatLayout EvaluateVortex(const FVibH2OVortexParams& Params, int32 Ordinal, int32 Count, int32 Seed, float TimeSeconds, float CollectiveSynchrony, float IndividualSynchrony);

	// ------------------------------------------------------ Always active

	/**
	 * The permanent float. Every bubble has its own phase, derived from its
	 * seed: without that, they all undulate in unison and the whole thing looks
	 * mechanical.
	 */
	static FVector EvaluateFloatOffset(const FVibH2OFloatParams& Params, int32 Seed, float TimeSeconds);
	static FRotator EvaluateFloatRotation(const FVibH2OFloatParams& Params, int32 Seed, float TimeSeconds);

	/**
	 * Arousal-driven drift, bounded by construction.
	 *
	 * @param MinSpacing The smaller of the two grid spacings. The drift is
	 *        clamped to MaxRatio times this value.
	 */
	static FVector EvaluateDriftOffset(const FVibH2ODriftParams& Params, int32 Seed, float TimeSeconds, float Excitation, float MinSpacing);

	/** Normalised drift amount, exactly 0 below the threshold. Exposed for tests. */
	static float ComputeDriftAmount(float Excitation, float Threshold);

	// ------------------------------------------------------------- Beat

	/**
	 * Advances the beat phase by one frame.
	 *
	 * The phase advances by DeltaTime * (Bpm / 60) and decrements by 1 as it
	 * crosses 1. A change of BPM only changes how fast it advances: no
	 * discontinuity, ever.
	 *
	 * @param OutBeats Number of beats crossed during this frame. May exceed 1
	 *        on a long frame or at a very high BPM.
	 * @return The new phase, within [0,1[.
	 */
	static float AdvanceBeatPhase(float CurrentPhase, float Bpm, float DeltaSeconds, int32& OutBeats);
};
