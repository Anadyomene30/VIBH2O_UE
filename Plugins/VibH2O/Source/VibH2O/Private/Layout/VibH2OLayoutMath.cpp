#include "Layout/VibH2OLayoutMath.h"

#include "Curves/RichCurve.h"

namespace VibH2OLayoutInternal
{
	/**
	 * Below this angle the room is treated as straight.
	 *
	 * The threshold is not a convenience: it avoids computing a curvature
	 * radius that tends to infinity, and it guarantees an *exact* return to the
	 * straight grid rather than an approximate one. The curved formula tends to
	 * the straight one continuously, so the switch is invisible.
	 */
	static constexpr float CurvatureEpsilonRad = 1.0e-6f;

	static float Smoothstep(float T)
	{
		return T * T * (3.0f - 2.0f * T);
	}
}

// ---------------------------------------------------------------- Noise

uint32 FVibH2ONoise::Hash(uint32 X)
{
	// 32-bit integer mixer, murmur-finalizer style. The constants are fixed on
	// purpose: the relief of the room must be identical everywhere.
	X ^= X >> 16;
	X *= 0x7FEB352Du;
	X ^= X >> 15;
	X *= 0x846CA68Bu;
	X ^= X >> 16;
	return X;
}

uint32 FVibH2ONoise::Hash2(int32 X, int32 Y)
{
	const uint32 A = static_cast<uint32>(X) * 0x9E3779B1u;
	const uint32 B = static_cast<uint32>(Y) * 0x85EBCA77u;
	return Hash(A ^ (B + 0x9E3779B9u + (A << 6) + (A >> 2)));
}

float FVibH2ONoise::UnitFloat(int32 Seed)
{
	return static_cast<float>(Hash(static_cast<uint32>(Seed))) / static_cast<float>(0xFFFFFFFFu);
}

float FVibH2ONoise::SignedFloat(int32 Seed)
{
	return UnitFloat(Seed) * 2.0f - 1.0f;
}

float FVibH2ONoise::Value2D(float X, float Y)
{
	const float FloorX = FMath::FloorToFloat(X);
	const float FloorY = FMath::FloorToFloat(Y);

	const int32 IX = static_cast<int32>(FloorX);
	const int32 IY = static_cast<int32>(FloorY);

	const float TX = VibH2OLayoutInternal::Smoothstep(X - FloorX);
	const float TY = VibH2OLayoutInternal::Smoothstep(Y - FloorY);

	auto Corner = [](int32 CX, int32 CY) -> float
	{
		return static_cast<float>(Hash2(CX, CY)) / static_cast<float>(0xFFFFFFFFu) * 2.0f - 1.0f;
	};

	const float V00 = Corner(IX, IY);
	const float V10 = Corner(IX + 1, IY);
	const float V01 = Corner(IX, IY + 1);
	const float V11 = Corner(IX + 1, IY + 1);

	const float Bottom = FMath::Lerp(V00, V10, TX);
	const float Top = FMath::Lerp(V01, V11, TX);
	return FMath::Lerp(Bottom, Top, TY);
}

// ----------------------------------------------------------- Seat index

bool FVibH2OLayoutMath::SeatIndexToColumnRow(int32 SeatIndex, int32 Columns, int32 Rows, EVibH2OSeatOrder Order, int32& OutColumn, int32& OutRow)
{
	OutColumn = 0;
	OutRow = 0;

	if (Columns <= 0 || Rows <= 0)
	{
		return false;
	}
	if (SeatIndex < 1 || SeatIndex > Columns * Rows)
	{
		return false;
	}

	const int32 Zero = SeatIndex - 1;
	if (Order == EVibH2OSeatOrder::ColumnMajor)
	{
		// index = column * rows + row + 1
		// This is the formula from Scripts/FormatRoomMapping.js on the Max side.
		OutColumn = Zero / Rows;
		OutRow = Zero % Rows;
	}
	else
	{
		// index = row * columns + column + 1
		OutRow = Zero / Columns;
		OutColumn = Zero % Columns;
	}
	return true;
}

int32 FVibH2OLayoutMath::ColumnRowToSeatIndex(int32 Column, int32 Row, int32 Columns, int32 Rows, EVibH2OSeatOrder Order)
{
	if (Columns <= 0 || Rows <= 0)
	{
		return 0;
	}
	if (Column < 0 || Column >= Columns || Row < 0 || Row >= Rows)
	{
		return 0;
	}

	return (Order == EVibH2OSeatOrder::ColumnMajor)
		? (Column * Rows + Row + 1)
		: (Row * Columns + Column + 1);
}

// ----------------------------------------------------------- Tableau 1

FVibH2OSeatLayout FVibH2OLayoutMath::EvaluateGrid(const FVibH2OGridParams& Params, int32 Column, int32 Row)
{
	using namespace VibH2OLayoutInternal;

	FVibH2OSeatLayout Result;

	const int32 Columns = FMath::Max(Params.Columns, 1);
	const int32 Rows = FMath::Max(Params.Rows, 1);

	// Offsets from the centre of the room, in seat steps. The room is thereby
	// centred on the stage root, which makes moving and scaling it
	// predictable.
	const float ColumnOffset = static_cast<float>(Column) - 0.5f * static_cast<float>(Columns - 1);
	const float RowOffset = static_cast<float>(Row) - 0.5f * static_cast<float>(Rows - 1);

	// Depth: the row offset plus the noise-driven relief. The noise is injected
	// here, before the curvature, so the relief follows the curve of the room
	// instead of being laid flat on top of it.
	float Depth = RowOffset * Params.RowSpacing;
	if (!FMath::IsNearlyZero(Params.DepthAmplitude))
	{
		const float Noise = FVibH2ONoise::Value2D(
			static_cast<float>(Column) * Params.DepthNoiseScale,
			static_cast<float>(Row) * Params.DepthNoiseScale);
		Depth += Noise * Params.DepthAmplitude;
	}

	const float ThetaRad = FMath::DegreesToRadians(Params.CurvatureAngleDeg);

	float X = 0.0f;
	float Y = 0.0f;
	float YawDeg = 0.0f;

	if (FMath::Abs(ThetaRad) < CurvatureEpsilonRad)
	{
		// A straight room, in the strict sense.
		X = Depth;
		Y = ColumnOffset * Params.ColumnSpacing;
		YawDeg = 0.0f;
	}
	else
	{
		// Radius of the arc through the centre row, chosen so the lateral gap
		// between two neighbouring seats of that row is exactly ColumnSpacing:
		// Rho * Theta = ColumnSpacing.
		const float Rho = Params.ColumnSpacing / ThetaRad;
		const float Radius = Rho + Depth;

		float Angle = ColumnOffset * ThetaRad;
		if (!Params.bFanOut && FMath::Abs(Radius) > KINDA_SMALL_NUMBER)
		{
			// Constant spacing: correct the angle so the arc length stays
			// ColumnSpacing whatever the row. Without this correction the back
			// rows widen - which is precisely the fan-out behaviour.
			Angle *= Rho / Radius;
		}

		// Circle centred at (-Rho, 0). The seat sits at angle Angle on radius
		// Radius. As Theta tends to zero, Rho tends to infinity and these two
		// expressions tend exactly to the straight grid.
		X = -Rho + Radius * FMath::Cos(Angle);
		Y = Radius * FMath::Sin(Angle);

		if (Params.bOrientToCenter)
		{
			// Relative yaw: at zero curvature it is zero, so enabling
			// orientation does nothing on a straight room. On a curved one each
			// bubble turns by exactly its radial angle, which aligns the
			// directional striations with the curve.
			YawDeg = FMath::RadiansToDegrees(Angle);
		}
	}

	// Row rise. With no curve, Z is exactly Row * ElevationPerRow.
	float Z = Params.BaseHeight;
	if (!FMath::IsNearlyZero(Params.ElevationPerRow))
	{
		if (Params.ElevationCurve != nullptr && Rows > 1)
		{
			const float T = static_cast<float>(Row) / static_cast<float>(Rows - 1);
			Z += Params.ElevationCurve->Eval(T, T) * Params.ElevationPerRow * static_cast<float>(Rows - 1);
		}
		else
		{
			Z += static_cast<float>(Row) * Params.ElevationPerRow;
		}
	}

	Result.Location = FVector(X, Y, Z);
	Result.Rotation = FRotator(0.0f, YawDeg, 0.0f) + Params.OrientationOffset;
	return Result;
}

// ----------------------------------------------------------- Tableau 2

FVibH2OSeatLayout FVibH2OLayoutMath::EvaluateVortex(const FVibH2OVortexParams& Params, int32 Ordinal, int32 Count, int32 Seed, float TimeSeconds, float CollectiveSynchrony, float IndividualSynchrony)
{
	FVibH2OSeatLayout Result;

	// Rank among the occupied bubbles: the spiral stays continuous even when
	// the room has holes in it.
	const float T = (Count > 1) ? (static_cast<float>(Ordinal) / static_cast<float>(Count - 1)) : 0.0f;

	const float EffectiveSync = FMath::Lerp(
		FMath::Clamp(CollectiveSynchrony, 0.0f, 1.0f),
		FMath::Clamp(IndividualSynchrony, 0.0f, 1.0f),
		FMath::Clamp(Params.IndividualInfluence, 0.0f, 1.0f));

	// Disorder is the complement of synchrony: a tight spiral when the audience
	// locks together, scatter when it drifts apart.
	const float Loose = 1.0f - EffectiveSync;

	const float AngleJitter = FVibH2ONoise::SignedFloat(Seed * 3 + 1) * Loose * Params.AngleDispersion;
	const float RadiusJitter = FVibH2ONoise::SignedFloat(Seed * 3 + 2) * Loose * Params.RadiusDispersion;
	const float HeightJitter = FVibH2ONoise::SignedFloat(Seed * 3 + 3) * Loose * Params.HeightDispersion;

	const float Turns = T * Params.Turns + TimeSeconds * Params.SpinSpeed + AngleJitter;
	const float AngleRad = Turns * 2.0f * PI;

	const float Radius = FMath::Lerp(Params.BaseRadius, Params.TopRadius, T) * (1.0f + RadiusJitter);
	const float Z = Params.ZOffset + Params.Height * (T + HeightJitter);

	Result.Location = FVector(Radius * FMath::Cos(AngleRad), Radius * FMath::Sin(AngleRad), Z);

	// The bubble faces outward from the cone, tangent to the spiral.
	Result.Rotation = FRotator(0.0f, FMath::RadiansToDegrees(AngleRad), 0.0f);
	return Result;
}

// -------------------------------------------------------- Always active

FVector FVibH2OLayoutMath::EvaluateFloatOffset(const FVibH2OFloatParams& Params, int32 Seed, float TimeSeconds)
{
	// Three independent phases and three slightly different frequencies:
	// without them the offset would trace a line or a circle, and it shows.
	const float P0 = FVibH2ONoise::UnitFloat(Seed * 7 + 1) * 2.0f * PI;
	const float P1 = FVibH2ONoise::UnitFloat(Seed * 7 + 2) * 2.0f * PI;
	const float P2 = FVibH2ONoise::UnitFloat(Seed * 7 + 3) * 2.0f * PI;

	const float W = TimeSeconds * Params.Speed * 2.0f * PI;

	return FVector(
		FMath::Sin(W * 1.00f + P0),
		FMath::Sin(W * 0.83f + P1),
		FMath::Sin(W * 1.17f + P2)) * Params.Amplitude;
}

FRotator FVibH2OLayoutMath::EvaluateFloatRotation(const FVibH2OFloatParams& Params, int32 Seed, float TimeSeconds)
{
	const float P3 = FVibH2ONoise::UnitFloat(Seed * 7 + 4) * 2.0f * PI;
	const float P4 = FVibH2ONoise::UnitFloat(Seed * 7 + 5) * 2.0f * PI;
	const float P5 = FVibH2ONoise::UnitFloat(Seed * 7 + 6) * 2.0f * PI;

	const float W = TimeSeconds * Params.Speed * 2.0f * PI;

	return FRotator(
		FMath::Sin(W * 0.61f + P3) * Params.RotationAmount,
		FMath::Sin(W * 0.47f + P5) * Params.RotationAmount * 0.5f,
		FMath::Sin(W * 0.79f + P4) * Params.RotationAmount);
}

float FVibH2OLayoutMath::ComputeDriftAmount(float Excitation, float Threshold)
{
	const float ClampedThreshold = FMath::Clamp(Threshold, 0.0f, 0.999f);
	// saturate((E - S) / (1 - S)): exactly zero below the threshold, exactly
	// one at full arousal, linear in between.
	return FMath::Clamp((Excitation - ClampedThreshold) / (1.0f - ClampedThreshold), 0.0f, 1.0f);
}

FVector FVibH2OLayoutMath::EvaluateDriftOffset(const FVibH2ODriftParams& Params, int32 Seed, float TimeSeconds, float Excitation, float MinSpacing)
{
	const float Amount = ComputeDriftAmount(Excitation, Params.Threshold);
	if (Amount <= 0.0f)
	{
		// Below the threshold: complete stillness, not a residue of movement.
		return FVector::ZeroVector;
	}

	// A direction of the bubble's own, turning slowly so the drift is not a
	// frozen translation.
	const float BasePhase = FVibH2ONoise::UnitFloat(Seed * 11 + 1) * 2.0f * PI;
	const float ZPhase = FVibH2ONoise::UnitFloat(Seed * 11 + 2) * 2.0f * PI;
	const float Angle = BasePhase + TimeSeconds * Params.DirectionSpeed * 2.0f * PI;

	FVector Direction(
		FMath::Cos(Angle),
		FMath::Sin(Angle),
		FMath::Sin(ZPhase + TimeSeconds * Params.DirectionSpeed * PI) * 0.4f);
	Direction.Normalize();

	FVector Offset = Direction * (Amount * Params.Intensity);

	// Hard clamp. With MaxRatio under 0.5 the bubble cannot reach the
	// neighbouring cell, whatever intensity has been dialled in.
	const float MaxLength = FMath::Max(MinSpacing, 0.0f) * FMath::Clamp(Params.MaxRatio, 0.0f, 0.5f);
	if (MaxLength > 0.0f && Offset.SizeSquared() > static_cast<double>(MaxLength) * MaxLength)
	{
		Offset = Offset.GetSafeNormal() * MaxLength;
	}

	return Offset;
}

// ----------------------------------------------------------------- Beat

float FVibH2OLayoutMath::AdvanceBeatPhase(float CurrentPhase, float Bpm, float DeltaSeconds, int32& OutBeats)
{
	OutBeats = 0;

	if (!FMath::IsFinite(CurrentPhase))
	{
		CurrentPhase = 0.0f;
	}
	if (Bpm <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return FMath::Frac(FMath::Max(CurrentPhase, 0.0f));
	}

	// The phase advances at the BPM's rate. Changing the BPM changes that rate
	// and nothing else: the phase itself is never rewritten, which is the whole
	// reason for not using a timer.
	float Phase = CurrentPhase + DeltaSeconds * (Bpm / 60.0f);

	if (Phase >= 1.0f)
	{
		// A long frame or a high BPM can cross several beats at once. Count
		// them all rather than lose any.
		const float Whole = FMath::FloorToFloat(Phase);
		OutBeats = static_cast<int32>(Whole);
		Phase -= Whole;
	}

	return Phase;
}
