#include "Steering/VibH2OSteeringMath.h"

#include "Layout/VibH2OLayoutMath.h"

namespace
{
	/**
	 * omega * t at which a critically damped spring has covered 99% of the
	 * way: the root of (1 + x) e^-x = 0.01.
	 */
	constexpr float SettleOmegaTimesSeconds = 6.6384f;

	/** Below these, the spring is at rest and snaps onto its target. */
	constexpr float RestDistance = 1.0e-4f;
	constexpr float RestVelocity = 1.0e-3f;

	float WrapAngle(float Radians)
	{
		return FMath::UnwindRadians(Radians);
	}
}

// ------------------------------------------------------------------- Spring

void FVibH2OSpring::Step(float Target, float SettleSeconds, float DeltaSeconds)
{
	if (SettleSeconds <= KINDA_SMALL_NUMBER)
	{
		Reset(Target);
		return;
	}
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	// The closed-form solution of x'' = -2w x' - w^2 x, stepped exactly: no
	// overshoot and no instability, whether the game runs at 30 or 300 frames
	// per second, or hitches for half a second during a shader compile.
	const float Omega = SettleOmegaTimesSeconds / SettleSeconds;
	const float X0 = Value - Target;
	const float Damping = FMath::Exp(-Omega * DeltaSeconds);
	const float Term = (Velocity + Omega * X0) * DeltaSeconds;

	const float X1 = (X0 + Term) * Damping;
	Velocity = (Velocity - Omega * Term) * Damping;
	Value = Target + X1;

	if (FMath::Abs(X1) < RestDistance && FMath::Abs(Velocity) < RestVelocity)
	{
		// Landed. Without the snap a released attractor would leave every
		// bubble a hair off its place forever.
		Reset(Target);
	}
}

// ---------------------------------------------------------------- Selection

float FVibH2OSteeringMath::SelectionHash(int32 Seed)
{
	return FVibH2ONoise::UnitFloat(Seed * 7919 + 101);
}

bool FVibH2OSteeringMath::IsInShare(float Hash, float Start, float Share)
{
	if (Share >= 1.0f)
	{
		return true;
	}
	if (Share <= 0.0f)
	{
		return false;
	}
	const float Offset = FMath::Frac(Hash - FMath::Frac(Start));
	return Offset < Share;
}

float FVibH2OSteeringMath::StaggeredDuration(float Nominal, int32 Seed, float Stagger)
{
	const float Spread = FMath::Clamp(Stagger, 0.0f, 1.0f);
	const float U = FVibH2ONoise::UnitFloat(Seed * 131 + 17);
	return FMath::Max(Nominal, 0.0f) * (1.0f + Spread * (U - 0.5f));
}

// -------------------------------------------------------------------- Cloud

FVector FVibH2OSteeringMath::CloudPoint(int32 Seed, float TimeSeconds, float BreathAmount, float BreathSpeed)
{
	const float U1 = FVibH2ONoise::UnitFloat(Seed * 37 + 1);
	const float U2 = FVibH2ONoise::UnitFloat(Seed * 37 + 2);
	const float U3 = FVibH2ONoise::UnitFloat(Seed * 37 + 3);
	const float U4 = FVibH2ONoise::UnitFloat(Seed * 37 + 4);

	// Uniform direction: uniform height on the sphere, uniform azimuth.
	const float Z = 2.0f * U1 - 1.0f;
	const float Ring = FMath::Sqrt(FMath::Max(1.0f - Z * Z, 0.0f));
	const float Azimuth = 2.0f * PI * U2;
	const FVector Direction(Ring * FMath::Cos(Azimuth), Ring * FMath::Sin(Azimuth), Z);

	// Cube root: uniform in volume, not crowded at the centre.
	const float Breath = FMath::Clamp(BreathAmount, 0.0f, 0.9f);
	const float Swell = 1.0f + Breath * FMath::Sin(2.0f * PI * (BreathSpeed * TimeSeconds + U4));
	const float Radius = FMath::Pow(U3, 1.0f / 3.0f) * Swell / (1.0f + Breath);

	return Direction * Radius;
}

FVector FVibH2OSteeringMath::BlendTowardTargets(const FVector& Base, TArrayView<const FVector> Targets, TArrayView<const float> Weights)
{
	const int32 Count = FMath::Min(Targets.Num(), Weights.Num());

	float TotalWeight = 0.0f;
	FVector WeightedSum = FVector::ZeroVector;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Weight = FMath::Max(Weights[Index], 0.0f);
		TotalWeight += Weight;
		WeightedSum += Targets[Index] * Weight;
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return Base;
	}

	const FVector Mean = WeightedSum / TotalWeight;
	return FMath::Lerp(Base, Mean, FMath::Min(TotalWeight, 1.0f));
}

// ---------------------------------------------------------------- Exclusion

float FVibH2OSteeringMath::ShapeNorm(const FVector2D& Point, const FVector2D& HalfSize, EVibH2OZoneShapeKind Shape)
{
	const double X = FMath::Abs(Point.X / FMath::Max(HalfSize.X, 1.0));
	const double Y = FMath::Abs(Point.Y / FMath::Max(HalfSize.Y, 1.0));

	if (Shape == EVibH2OZoneShapeKind::Rectangle)
	{
		const double X2 = X * X;
		const double Y2 = Y * Y;
		return static_cast<float>(FMath::Pow(X2 * X2 * X2 + Y2 * Y2 * Y2, 1.0 / 6.0));
	}
	return static_cast<float>(FMath::Sqrt(X * X + Y * Y));
}

float FVibH2OSteeringMath::SoftOutside(float SignedDistance, float Margin)
{
	if (Margin <= KINDA_SMALL_NUMBER)
	{
		return FMath::Max(SignedDistance, 0.0f);
	}
	if (SignedDistance >= Margin)
	{
		return SignedDistance;
	}
	// Equal to Margin with a slope of 1 at Margin, positive everywhere, and
	// toward 0 deep inside: the bubble is eased onto the edge, never past it.
	return Margin * FMath::Exp((SignedDistance - Margin) / Margin);
}

FVector2D FVibH2OSteeringMath::PushOutOfZone(const FVector2D& Point, const FVibH2OZonePushParams& Params,
	FVibH2OZoneBubbleState& State, int32 Seed, float DeltaSeconds)
{
	const FVector2D HalfSize(FMath::Max(Params.HalfSize.X, 1.0), FMath::Max(Params.HalfSize.Y, 1.0));
	const FVector2D Unit(Point.X / HalfSize.X, Point.Y / HalfSize.Y);
	const double UnitLength = Unit.Size();

	// The side the bubble really is on. At the exact centre there is none:
	// keep the remembered one, or failing that a side of its own.
	FVector2D Actual;
	if (UnitLength > 1.0e-5)
	{
		Actual = Unit / UnitLength;
	}
	else if (State.bHasDirection)
	{
		Actual = State.Direction;
	}
	else
	{
		const float Angle = 2.0f * PI * FVibH2ONoise::UnitFloat(Seed * 53 + 9);
		Actual = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	}

	// The remembered side follows the real one, at a bounded angular speed.
	if (!State.bHasDirection)
	{
		State.Direction = Actual;
		State.bHasDirection = true;
	}
	else
	{
		const float Current = FMath::Atan2(State.Direction.Y, State.Direction.X);
		const float Wanted = FMath::Atan2(Actual.Y, Actual.X);
		float Delta = WrapAngle(Wanted - Current);
		if (FMath::Abs(Delta) > PI - 1.0e-3f)
		{
			// Exactly opposite: either way round is as short. Let the seed
			// choose, so a crowd crossing the zone parts both ways.
			Delta = (Seed & 1) ? FMath::Abs(Delta) : -FMath::Abs(Delta);
		}
		const float MaxStep = FMath::Max(Params.GlideRadiansPerSecond, 0.0f) * FMath::Max(DeltaSeconds, 0.0f);
		const float Angle = Current + FMath::Clamp(Delta, -MaxStep, MaxStep);
		State.Direction = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	}

	const float Weight = FMath::Clamp(Params.Weight, 0.0f, 1.0f);
	if (Weight <= 0.0f)
	{
		return Point;
	}

	const float Norm = ShapeNorm(Point, HalfSize, Params.Shape);
	const float Signed = Norm - 1.0f;
	const float Margin = FMath::Max(Params.Margin, 0.0f) / static_cast<float>(FMath::Min(HalfSize.X, HalfSize.Y));

	if (Signed >= Margin)
	{
		return Point;
	}

	// Inside the shape, the remembered side alone; across the soft band it
	// hands over to the real one, which it equals at the outer edge - so the
	// push fades in without a seam.
	const float Sticky = (Signed <= 0.0f) ? 1.0f : 1.0f - FMath::SmoothStep(0.0f, Margin, Signed);
	const float ActualAngle = FMath::Atan2(Actual.Y, Actual.X);
	const float StateAngle = FMath::Atan2(State.Direction.Y, State.Direction.X);
	const float Angle = ActualAngle + Sticky * WrapAngle(StateAngle - ActualAngle);
	const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));

	// Back to the shape: the point along Direction whose norm is the eased one.
	const float TargetNorm = 1.0f + SoftOutside(Signed, Margin);
	const float DirectionNorm = FMath::Max(ShapeNorm(Direction, FVector2D(1.0, 1.0), Params.Shape), KINDA_SMALL_NUMBER);
	const FVector2D PushedUnit = Direction * (TargetNorm / DirectionNorm);
	const FVector2D Pushed(PushedUnit.X * HalfSize.X, PushedUnit.Y * HalfSize.Y);

	return Point + (Pushed - Point) * static_cast<double>(Weight);
}
