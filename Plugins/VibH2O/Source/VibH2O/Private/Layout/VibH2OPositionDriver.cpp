#include "Layout/VibH2OPositionDriver.h"

#include "Curves/CurveFloat.h"
#include "Layout/VibH2OLayoutMath.h"
#include "Layout/VibH2OSchoolMath.h"
#include "VibH2OStageActor.h"

// --------------------------------------------------------------------- Base

FTransform UVibH2OPositionDriver::EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const
{
	return FTransform::Identity;
}

void UVibH2OPositionDriver::PrepareFrame_Implementation(const FVibH2OFrameContext& Frame)
{
}

FText UVibH2OPositionDriver::GetDisplayName_Implementation() const
{
	return NSLOCTEXT("VibH2O", "DriverGeneric", "Pilote de position");
}

// ---------------------------------------------------------------- Tableau 1

FTransform UVibH2OGridDriver::EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const
{
	const AVibH2OStageActor* Stage = Context.Stage;
	if (Stage == nullptr)
	{
		return FTransform::Identity;
	}

	FVibH2OGridParams Params;
	Params.Columns = Context.Columns;
	Params.Rows = Context.Rows;
	Params.ColumnSpacing = Stage->ColumnSpacing;
	Params.RowSpacing = Stage->RowSpacing;
	Params.CurvatureAngleDeg = Stage->CurvatureAngle;
	Params.bFanOut = Stage->bFanOut;
	Params.bOrientToCenter = Stage->bOrientToCenter;
	Params.OrientationOffset = Stage->OrientationOffset;
	Params.ElevationPerRow = Stage->ElevationPerRow;
	Params.ElevationCurve = Stage->ElevationCurve ? &Stage->ElevationCurve->FloatCurve : nullptr;
	Params.DepthAmplitude = Stage->DepthAmplitude;
	Params.DepthNoiseScale = Stage->DepthNoiseScale;

	const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateGrid(Params, Context.Seat.Column, Context.Seat.Row);
	return FTransform(Layout.Rotation, Layout.Location);
}

FText UVibH2OGridDriver::GetDisplayName_Implementation() const
{
	return NSLOCTEXT("VibH2O", "DriverGrid", "Tableau 1 - Plan de salle");
}

// ---------------------------------------------------------------- Tableau 2

FTransform UVibH2OVortexDriver::EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const
{
	const AVibH2OStageActor* Stage = Context.Stage;
	if (Stage == nullptr)
	{
		return FTransform::Identity;
	}

	FVibH2OVortexParams Params;
	Params.Height = Stage->VortexHeight;
	Params.BaseRadius = Stage->VortexBaseRadius;
	Params.TopRadius = Stage->VortexTopRadius;
	Params.Turns = Stage->VortexTurns;
	Params.SpinSpeed = Stage->VortexSpinSpeed;
	Params.AngleDispersion = Stage->VortexAngleDispersion;
	Params.RadiusDispersion = Stage->VortexRadiusDispersion;
	Params.HeightDispersion = Stage->VortexHeightDispersion;
	Params.IndividualInfluence = Stage->VortexIndividualInfluence;
	Params.ZOffset = Stage->VortexZOffset;

	// Daughters take their own place on the spiral, next to their mother's.
	const int32 FamilySize = FMath::Max(Context.FamilySize, 1);
	const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateVortex(
		Params,
		Context.OccupiedOrdinal * FamilySize + FMath::Clamp(Context.FamilyMember, 0, FamilySize - 1),
		Context.OccupiedCount * FamilySize,
		Context.Seed,
		Context.TimeSeconds,
		Context.CollectiveSynchrony,
		Context.Individual.Synchrony);

	return FTransform(Layout.Rotation, Layout.Location);
}

FText UVibH2OVortexDriver::GetDisplayName_Implementation() const
{
	return NSLOCTEXT("VibH2O", "DriverVortex", "Tableau 2 - Vortex");
}

// ------------------------------------------------------- Tableau 2, boids

namespace
{
	/** Packs a cell coordinate into one key, 21 bits per axis. */
	uint64 PackCell(int32 X, int32 Y, int32 Z)
	{
		constexpr uint64 Mask = (1ull << 21) - 1;
		constexpr int32 Bias = 1 << 20;
		return ((static_cast<uint64>(X + Bias) & Mask))
			| ((static_cast<uint64>(Y + Bias) & Mask) << 21)
			| ((static_cast<uint64>(Z + Bias) & Mask) << 42);
	}

	/**
	 * Height along the cone, 0 to 1, for a uniform U in [0,1], such that the
	 * fish cover the wall evenly: the top ring is longer than the bottom one,
	 * so it needs more fish. Inverse of the cumulative area of the frustum.
	 */
	float ConeHeightForUniform(float U, float BaseRadius, float TopRadius)
	{
		const float A = (TopRadius - BaseRadius) * 0.5f;
		const float B = BaseRadius;
		const float Area = U * (BaseRadius + (TopRadius - BaseRadius) * 0.5f);
		if (FMath::Abs(A) < KINDA_SMALL_NUMBER)
		{
			return U;
		}
		const float Disc = FMath::Max(B * B + 4.0f * A * Area, 0.0f);
		return FMath::Clamp((-B + FMath::Sqrt(Disc)) / (2.0f * A), 0.0f, 1.0f);
	}
}

FVector UVibH2OFlockDriver::FamilyHome(int32 Family, int32 FamilyCount, float TimeSeconds) const
{
	// Golden-angle sampling of the disc: the families share the volume evenly,
	// which is what keeps a scattered school readable. A random scatter would
	// clump some families together and leave holes elsewhere.
	const float N = static_cast<float>(FMath::Max(FamilyCount, 1));
	const float Angle = Family * 2.39996323f + FVibH2ONoise::UnitFloat(Family * 29 + 1) * 0.6f;
	const float Radius = FMath::Sqrt((Family + 0.5f) / N) * BoundsRadius * 0.8f;
	const float Z = ZOffset + FVibH2ONoise::SignedFloat(Family * 29 + 2) * BoundsHeight * 0.38f;

	// Each place drifts slowly on its own: the constellation breathes.
	const float W = TimeSeconds * FamilyDriftSpeed * 2.0f * PI;
	const FVector Drift(
		FMath::Sin(W + FVibH2ONoise::UnitFloat(Family * 29 + 3) * 2.0f * PI),
		FMath::Cos(W * 1.13f + FVibH2ONoise::UnitFloat(Family * 29 + 4) * 2.0f * PI),
		0.5f * FMath::Sin(W * 0.87f + FVibH2ONoise::UnitFloat(Family * 29 + 5) * 2.0f * PI));

	return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Z) + Drift * BoundsRadius * 0.08f;
}

void UVibH2OFlockDriver::Reseed(int32 FamilyCount, int32 FamilySize)
{
	const int32 Count = FamilyCount * FamilySize;
	Positions.SetNumUninitialized(Count);
	Velocities.SetNumUninitialized(Count);

	for (int32 Family = 0; Family < FamilyCount; ++Family)
	{
		// Deterministic, and already gathered by family: a restart gives the
		// same school, and the first frame is already readable.
		const FVector Home = FamilyHome(Family, FamilyCount, 0.0f);
		for (int32 Member = 0; Member < FamilySize; ++Member)
		{
			const int32 Index = Family * FamilySize + Member;
			Positions[Index] = Home + FVector(
				FVibH2ONoise::SignedFloat(Index * 13 + 1),
				FVibH2ONoise::SignedFloat(Index * 13 + 2),
				FVibH2ONoise::SignedFloat(Index * 13 + 3)) * SeparationRadius;

			const float VA = FVibH2ONoise::UnitFloat(Index * 13 + 4) * 2.0f * PI;
			Velocities[Index] = FVector(FMath::Cos(VA), FMath::Sin(VA), FVibH2ONoise::SignedFloat(Index * 13 + 5) * 0.2f)
				* FMath::Lerp(MinSpeed, MaxSpeed, 0.5f);
		}
	}
	SeededFamilySize = FamilySize;
	bSeeded = true;
}

void UVibH2OFlockDriver::PrepareFrame_Implementation(const FVibH2OFrameContext& Frame)
{
	const AVibH2OStageActor* Stage = Frame.Stage;

	// The order follows the room even while the morph sits at zero, so that
	// the school comes on stage already in the state the room is in.
	float Target = FVibH2OSchoolMath::ComputeOrderTarget(Frame.CollectiveSynchrony, 0.35f, 0.70f);
	float Smoothing = 2.0f;
	if (Stage != nullptr)
	{
		const float FromSync = FVibH2OSchoolMath::ComputeOrderTarget(
			Frame.CollectiveSynchrony, Stage->SchoolDisperseBelow, Stage->SchoolVortexAbove);
		Target = FMath::Lerp(
			FMath::Clamp(Stage->SchoolManualOrder, 0.0f, 1.0f),
			FromSync,
			FMath::Clamp(Stage->SchoolSyncInfluence, 0.0f, 1.0f));
		Smoothing = Stage->SchoolOrderSmoothing;
	}
	CurrentOrder = bOrderPrimed
		? FVibH2OSchoolMath::SmoothOrder(CurrentOrder, Target, Frame.DeltaSeconds, Smoothing)
		: Target;
	bOrderPrimed = true;

	// The school only swims while it is on stage. Integrating it behind a
	// morph that sits at zero would burn the time budget on something nobody
	// can see, and would teleport the school forward when the morph opens.
	if (Frame.OccupiedCount <= 0 || Frame.BlendAlpha <= KINDA_SMALL_NUMBER)
	{
		bSeeded = false;
		return;
	}

	FVibH2OSchoolFrame School;
	School.DeltaSeconds = Frame.DeltaSeconds;
	School.TimeSeconds = Frame.TimeSeconds;
	School.FamilyCount = Frame.OccupiedCount;
	School.FamilySize = FMath::Max(Frame.FamilySize, 1);
	School.Order = CurrentOrder;
	School.AverageExcitation = Frame.AverageExcitation;
	if (Stage != nullptr)
	{
		School.ConeBaseRadius = Stage->VortexBaseRadius;
		School.ConeTopRadius = Stage->VortexTopRadius;
		School.ConeHeight = Stage->VortexHeight;
		School.ConeZOffset = Stage->VortexZOffset;
		School.SpinDirection = Stage->VortexSpinSpeed < 0.0f ? -1.0f : 1.0f;
	}
	StepSchool(School);
}

void UVibH2OFlockDriver::StepSchool(const FVibH2OSchoolFrame& School)
{
	const int32 FamilySize = FMath::Max(School.FamilySize, 1);
	const int32 FamilyCount = FMath::Max(School.FamilyCount, 0);
	const int32 Count = FamilyCount * FamilySize;
	if (Count <= 0)
	{
		bSeeded = false;
		Positions.Reset();
		Velocities.Reset();
		return;
	}

	if (!bSeeded || Positions.Num() != Count || SeededFamilySize != FamilySize)
	{
		Reseed(FamilyCount, FamilySize);
	}

	// Clamp the step: a hitch or a paused editor must not fling the school
	// across the volume in one frame.
	const float Dt = FMath::Clamp(School.DeltaSeconds, 0.0f, 0.05f);
	if (Dt <= 0.0f)
	{
		return;
	}

	const float Time = School.TimeSeconds;
	const float Order = FMath::Clamp(School.Order, 0.0f, 1.0f);
	const float Disorder = 1.0f - Order;

	// A dispersed school is a calmer one: slow families read, fast ones blur.
	const float Calm = FMath::Lerp(FMath::Clamp(DispersedSpeedFactor, 0.05f, 1.0f), 1.0f, Order);
	const float Speed = Calm * FMath::Lerp(MinSpeed, MaxSpeed,
		FMath::Clamp(0.35f + School.AverageExcitation * ExcitationSpeedBoost, 0.0f, 1.0f));
	const float FloorSpeed = MinSpeed * Calm;
	const float CeilingSpeed = FMath::Max(Speed, FloorSpeed + 1.0f);

	// Sisters share their mother's volume, so each needs less room than she
	// did: the personal space shrinks with the body.
	const float BodyScale = FMath::Pow(1.0f / static_cast<float>(FamilySize), 1.0f / 3.0f);
	const float SepRadius = FMath::Max(SeparationRadius * BodyScale, 1.0f);
	const float SepSq = SepRadius * SepRadius;
	const float NeighbourR = FMath::Max(NeighbourRadius, SepRadius);
	const float NeighbourSq = NeighbourR * NeighbourR;

	// Kept faint on purpose, and measured so: in order the cone holds the
	// school together, and full neighbour cohesion collapsed the vortex into a
	// travelling ball within fifteen seconds; dispersed, the neighbours that
	// count are the sisters, handled below. Alignment likewise stays low in
	// the vortex: at full weight it flattened every vertical move, and the
	// school bunched in the middle third of the cone.
	const float Cohesion = CohesionWeight * Disorder;
	const float Alignment = AlignmentWeight * FMath::Lerp(0.10f, 0.30f, Order);
	const float Wander = FMath::Lerp(0.7f, 0.15f, Order);

	// The centre of the school wanders; once in order it only sways, so the
	// vortex stays where the eye expects it.
	const float AT = Time * AttractorSpeed * 2.0f * PI;
	const float SwayRadius = BoundsRadius * FMath::Lerp(0.30f, FMath::Clamp(AxisSway, 0.0f, 1.0f), Order);
	const FVector Centre(FMath::Cos(AT) * SwayRadius, FMath::Sin(AT * 1.3f) * SwayRadius, 0.0f);

	const float ConeZ0 = School.ConeZOffset;
	const float ConeH = FMath::Max(School.ConeHeight, 1.0f);
	const float ConeRb = FMath::Max(School.ConeBaseRadius, 0.0f);
	const float ConeRt = FMath::Max(School.ConeTopRadius, 0.0f);

	// Containment covers both shapes, so it never fights either of them.
	const float RadiusLimit = FMath::Max(BoundsRadius, FMath::Max(ConeRb, ConeRt) * (1.0f + ShellThickness * 0.5f)) * 1.05f;
	const float ZLow = FMath::Min(ZOffset - BoundsHeight * 0.5f, ConeZ0) - 50.0f;
	const float ZHigh = FMath::Max(ZOffset + BoundsHeight * 0.5f, ConeZ0 + ConeH) + 50.0f;

	// Family centres and headings, once per frame.
	FamilyCentres.SetNumUninitialized(FamilyCount);
	FamilyVelocities.SetNumUninitialized(FamilyCount);
	for (int32 Family = 0; Family < FamilyCount; ++Family)
	{
		FVector C = FVector::ZeroVector;
		FVector V = FVector::ZeroVector;
		for (int32 Member = 0; Member < FamilySize; ++Member)
		{
			C += Positions[Family * FamilySize + Member];
			V += Velocities[Family * FamilySize + Member];
		}
		FamilyCentres[Family] = C / FamilySize;
		FamilyVelocities[Family] = V / FamilySize;
	}

	// Spatial hash, one cell per neighbour radius: each fish only looks at the
	// 27 cells around it. Seven hundred bodies tested pairwise would be half a
	// million distances a frame; this keeps it to the few that matter.
	const float InvCell = 1.0f / NeighbourR;
	auto CellCoord = [InvCell](double Value) { return FMath::FloorToInt32(static_cast<float>(Value) * InvCell); };

	CellKeys.SetNumUninitialized(Count);
	CellOrder.SetNumUninitialized(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector& P = Positions[Index];
		CellKeys[Index] = PackCell(CellCoord(P.X), CellCoord(P.Y), CellCoord(P.Z));
		CellOrder[Index] = Index;
	}
	{
		const TArray<uint64>& Keys = CellKeys;
		CellOrder.Sort([&Keys](int32 A, int32 B) { return Keys[A] < Keys[B]; });
	}
	CellRanges.Reset();
	for (int32 Start = 0; Start < Count;)
	{
		const uint64 Key = CellKeys[CellOrder[Start]];
		int32 End = Start + 1;
		while (End < Count && CellKeys[CellOrder[End]] == Key)
		{
			++End;
		}
		CellRanges.Add(Key, FIntPoint(Start, End - Start));
		Start = End;
	}

	NewVelocities.SetNumUninitialized(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Family = i / FamilySize;
		const FVector Pos = Positions[i];
		const FVector Vel = Velocities[i];

		FVector Separate = FVector::ZeroVector;
		FVector Align = FVector::ZeroVector;
		FVector NeighbourCentre = FVector::ZeroVector;
		int32 Neighbours = 0;

		const int32 CX = CellCoord(Pos.X);
		const int32 CY = CellCoord(Pos.Y);
		const int32 CZ = CellCoord(Pos.Z);
		for (int32 DZ = -1; DZ <= 1; ++DZ)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const FIntPoint* Range = CellRanges.Find(PackCell(CX + DX, CY + DY, CZ + DZ));
					if (Range == nullptr)
					{
						continue;
					}
					for (int32 Slot = Range->X; Slot < Range->X + Range->Y; ++Slot)
					{
						const int32 j = CellOrder[Slot];
						if (j == i)
						{
							continue;
						}
						const FVector Delta = Positions[j] - Pos;
						const float DistSq = static_cast<float>(Delta.SizeSquared());
						if (DistSq > NeighbourSq || DistSq < KINDA_SMALL_NUMBER)
						{
							continue;
						}

						++Neighbours;
						Align += Velocities[j];
						NeighbourCentre += Positions[j];

						if (DistSq < SepSq)
						{
							// Zero at the edge of the personal space, rising
							// steeply inside it: bubbles do not interpenetrate.
							const float Dist = FMath::Sqrt(DistSq);
							Separate -= (Delta / Dist) * FMath::Min(SepRadius / Dist - 1.0f, 4.0f);
						}
					}
				}
			}
		}

		FVector Steer = FVector::ZeroVector;

		if (Neighbours > 0)
		{
			Align /= Neighbours;
			NeighbourCentre /= Neighbours;
			Steer += Separate * SeparationWeight * Speed;
			Steer += (Align.GetSafeNormal() * Speed - Vel).GetSafeNormal() * Alignment * Speed;
			Steer += (NeighbourCentre - Pos).GetSafeNormal() * Cohesion * Speed;
		}

		// ---- Order: the vortex. Everyone swims the same way round an axis,
		// held on the wall of a cone that opens upward.
		if (Order > KINDA_SMALL_NUMBER)
		{
			const FVector Rel(Pos.X - Centre.X, Pos.Y - Centre.Y, 0.0f);
			const float R = static_cast<float>(Rel.Size());
			const FVector Radial = R > 1.0f ? Rel / R : FVector(1.0f, 0.0f, 0.0f);
			const FVector Tangent = FVector(-Radial.Y, Radial.X, 0.0f) * School.SpinDirection;

			const float H01 = FMath::Clamp(static_cast<float>(Pos.Z - ConeZ0) / ConeH, 0.0f, 1.0f);
			const float ConeR = FMath::Lerp(ConeRb, ConeRt, H01);
			const float HalfShell = FMath::Max(ConeR * ShellThickness * 0.5f, SepRadius);
			// A spring onto the wall, softer for a thicker shell, plus the exact
			// centripetal pull a circle at this speed needs. Without the latter
			// every fish leaves along its tangent and the school drifts outward.
			const float Error = R - ConeR;
			Steer -= Radial * FMath::Clamp(Error / HalfShell, -2.0f, 2.0f) * ConeWeight * Speed * Order;
			Steer -= Radial * (Speed * Speed / FMath::Max(R, 50.0f)) * Order;

			// Steer the horizontal velocity onto the tangent: a first-order
			// pull, so the school settles into the turn rather than snapping.
			const FVector Horizontal(Vel.X, Vel.Y, 0.0f);
			Steer += (Tangent * Speed - Horizontal) * SwirlWeight * Order;

			// Every fish has its own height on the cone, chosen so the wall is
			// evenly covered, and drifts slowly up and down around it.
			const float Base = ConeHeightForUniform(FVibH2ONoise::UnitFloat(i * 17 + 1), ConeRb, ConeRt);
			const float Cycle = 2.0f * PI * VerticalCycleSpeed * (0.7f + 0.6f * FVibH2ONoise::UnitFloat(i * 17 + 2));
			const float Wave = 0.1f * FMath::Sin(Time * Cycle + FVibH2ONoise::UnitFloat(i * 17 + 3) * 2.0f * PI);
			const float TargetZ = ConeZ0 + ConeH * FMath::Clamp(Base + Wave, 0.0f, 1.0f);
			Steer.Z += FMath::Clamp(static_cast<float>(TargetZ - Pos.Z) / (ConeH * 0.15f), -1.0f, 1.0f)
				* VerticalWeight * Speed * Order;
		}

		// ---- Disorder: the families. Each one keeps its sisters close and
		// its own place in the volume.
		if (Disorder > KINDA_SMALL_NUMBER)
		{
			const FVector Home = Centre + FamilyHome(Family, FamilyCount, Time);
			const float HomeSoft = FMath::Max(BoundsRadius * 0.2f, 1.0f);
			Steer += ((Home - Pos) / HomeSoft).GetClampedToMaxSize(1.0f) * AttractorWeight * Speed * Disorder;

			if (FamilySize > 1)
			{
				const float KinSoft = SepRadius * 2.0f;
				Steer += ((FamilyCentres[Family] - Pos) / KinSoft).GetClampedToMaxSize(1.0f) * KinWeight * Speed * Disorder;
				// The family swims as one small school: same heading.
				Steer += (FamilyVelocities[Family] - Vel) * 0.8f * Disorder;
			}
		}

		// A little wander of its own, so the school looks alive rather than
		// merely arranged.
		const float WT = Time * 0.6f;
		Steer += FVector(
			FMath::Sin(WT + i * 1.7f),
			FMath::Cos(WT * 1.1f + i * 2.3f),
			FMath::Sin(WT * 0.8f + i * 3.1f) * 0.5f) * Wander * Speed * 0.6f;

		// Containment: a soft push back once past the shell, so the school
		// turns rather than bounces.
		const FVector Flat(Pos.X, Pos.Y, 0.0f);
		const float Radial = static_cast<float>(Flat.Size());
		if (Radial > RadiusLimit)
		{
			Steer -= Flat.GetSafeNormal() * (Radial - RadiusLimit) * 4.0f;
		}
		if (Pos.Z < ZLow)
		{
			Steer.Z += static_cast<float>(ZLow - Pos.Z) * 4.0f;
		}
		else if (Pos.Z > ZHigh)
		{
			Steer.Z -= static_cast<float>(Pos.Z - ZHigh) * 4.0f;
		}

		FVector V = Vel + Steer * Dt;

		// Bounded on both sides: a fish never stops dead, and never teleports.
		const float S = static_cast<float>(V.Size());
		if (S < KINDA_SMALL_NUMBER)
		{
			V = FVector(1.0f, 0.0f, 0.0f) * FloorSpeed;
		}
		else
		{
			V = V / S * FMath::Clamp(S, FloorSpeed, CeilingSpeed);
		}
		NewVelocities[i] = V;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		Velocities[i] = NewVelocities[i];
		Positions[i] += Velocities[i] * Dt;
	}
}

FTransform UVibH2OFlockDriver::EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const
{
	const int32 FamilySize = FMath::Max(Context.FamilySize, 1);
	const int32 Member = FMath::Clamp(Context.FamilyMember, 0, FamilySize - 1);
	const int32 Index = Context.OccupiedOrdinal * FamilySize + Member;
	if (FamilySize != SeededFamilySize || !Positions.IsValidIndex(Index))
	{
		return FTransform::Identity;
	}

	// Facing where it swims is what reads as a fish rather than a drifting
	// particle - the single cheapest cue in the whole behaviour.
	const FVector V = Velocities.IsValidIndex(Index) ? Velocities[Index] : FVector::ForwardVector;
	const FRotator Rotation = V.SizeSquared() > KINDA_SMALL_NUMBER
		? V.Rotation()
		: FRotator::ZeroRotator;

	return FTransform(Rotation, Positions[Index]);
}

FText UVibH2OFlockDriver::GetDisplayName_Implementation() const
{
	return NSLOCTEXT("VibH2O", "DriverFlock", "Tableau 2 - Banc de poissons");
}
