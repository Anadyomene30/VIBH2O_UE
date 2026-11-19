#include "Layout/VibH2OSchoolMath.h"

#include "Layout/VibH2OLayoutMath.h"

namespace
{
	/** Hermite smoothstep over [Edge0, Edge1], clamped. */
	float SmoothStep01(float Edge0, float Edge1, float X)
	{
		if (Edge1 - Edge0 <= KINDA_SMALL_NUMBER)
		{
			return X >= Edge0 ? 1.0f : 0.0f;
		}
		const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Upper bound on the family size: eight daughters, nine sisters. */
	constexpr int32 MaxDaughters = 8;
}

int32 FVibH2OSchoolMath::ComputeFamilySize(int32 DaughtersPerBubble, int32 OccupiedCount, int32 MaxSchoolSize)
{
	int32 Daughters = FMath::Clamp(DaughtersPerBubble, 0, MaxDaughters);
	if (OccupiedCount > 0 && MaxSchoolSize > 0)
	{
		// Integer division on purpose: the ceiling is a ceiling, never exceeded.
		Daughters = FMath::Min(Daughters, MaxSchoolSize / OccupiedCount - 1);
	}
	return FMath::Max(Daughters, 0) + 1;
}

float FVibH2OSchoolMath::ComputeDivisionProgress(float Blend, float Start, float End)
{
	if (End - Start <= KINDA_SMALL_NUMBER)
	{
		return Blend >= Start ? 1.0f : 0.0f;
	}
	return FMath::Clamp((Blend - Start) / (End - Start), 0.0f, 1.0f);
}

float FVibH2OSchoolMath::ComputeMigrationBlend(float Blend, float Start, float End, int32 FamilySize)
{
	const float Clamped = FMath::Clamp(Blend, 0.0f, 1.0f);
	if (FamilySize <= 1)
	{
		return Clamped;
	}

	const float Departure = FMath::Clamp(Start + FMath::Max(End - Start, 0.0f) * 0.5f, 0.0f, 1.0f);
	if (Departure >= 1.0f - KINDA_SMALL_NUMBER)
	{
		return Clamped >= 1.0f ? 1.0f : 0.0f;
	}
	return FMath::Clamp((Clamped - Departure) / (1.0f - Departure), 0.0f, 1.0f);
}

FVibH2ODivisionPose FVibH2OSchoolMath::EvaluateDivision(int32 Member, int32 FamilySize, int32 FamilySeed, float Progress, float Spread)
{
	FVibH2ODivisionPose Pose;
	if (FamilySize <= 1 || Member < 0 || Member >= FamilySize)
	{
		return Pose;
	}

	const float P = FMath::Clamp(Progress, 0.0f, 1.0f);
	const float Sisters = static_cast<float>(FamilySize);
	const float Daughters = Sisters - 1.0f;

	// Two overlapping gestures: the daughters swell first, nested inside the
	// mother, and the sisters part a little later. Budding, then separation.
	const float Grow = SmoothStep01(0.0f, 0.7f, P);
	const float Part = SmoothStep01(0.15f, 1.0f, P);

	// Equal sizes at the end, and the family's volume conserved throughout:
	// mother^3 + daughters * daughter^3 = 1 for every P.
	const float Equal = FMath::Pow(1.0f / Sisters, 1.0f / 3.0f);
	const float Daughter = Equal * Grow;
	if (Member == 0)
	{
		const float Remaining = FMath::Max(1.0f - Daughters * Daughter * Daughter * Daughter, 0.0f);
		Pose.ScaleFactor = FMath::Pow(Remaining, 1.0f / 3.0f);
	}
	else
	{
		Pose.ScaleFactor = Daughter;
	}
	Pose.Individuation = Part;

	// Sisters spread over a sphere (Fibonacci lattice), recentred so the family
	// stays centred on its seat - for two sisters the points are antipodal.
	auto Direction = [Sisters](int32 Index)
	{
		const float Y = 1.0f - 2.0f * (static_cast<float>(Index) + 0.5f) / Sisters;
		const float R = FMath::Sqrt(FMath::Max(1.0f - Y * Y, 0.0f));
		const float Theta = static_cast<float>(Index) * 2.39996323f; // Golden angle.
		return FVector(R * FMath::Cos(Theta), R * FMath::Sin(Theta), Y);
	};

	FVector Mean = FVector::ZeroVector;
	for (int32 Index = 0; Index < FamilySize; ++Index)
	{
		Mean += Direction(Index);
	}
	Mean /= Sisters;

	// Each family turned its own way: a room dividing in lockstep would read as
	// a machine, not as life.
	const FRotator Turn(
		FVibH2ONoise::SignedFloat(FamilySeed * 11 + 1) * 70.0f,
		FVibH2ONoise::UnitFloat(FamilySeed * 11 + 2) * 360.0f,
		FVibH2ONoise::SignedFloat(FamilySeed * 11 + 3) * 70.0f);

	Pose.Offset = Turn.RotateVector(Direction(Member) - Mean) * (Spread * Part);
	return Pose;
}

float FVibH2OSchoolMath::ComputeOrderTarget(float Synchrony, float DisperseBelow, float VortexAbove)
{
	return SmoothStep01(DisperseBelow, VortexAbove, FMath::Clamp(Synchrony, 0.0f, 1.0f));
}

float FVibH2OSchoolMath::SmoothOrder(float Current, float Target, float DeltaSeconds, float TimeConstant)
{
	if (TimeConstant <= KINDA_SMALL_NUMBER)
	{
		return Target;
	}
	const float Alpha = 1.0f - FMath::Exp(-FMath::Max(DeltaSeconds, 0.0f) / TimeConstant);
	return Current + (Target - Current) * Alpha;
}

int32 FVibH2OSchoolMath::MakeMemberSeed(int32 SeatIndex, int32 Member)
{
	// A prime stride far beyond any seat count: a daughter's seed never lands
	// on another seat's.
	return Member <= 0 ? SeatIndex : SeatIndex + Member * 100003;
}
