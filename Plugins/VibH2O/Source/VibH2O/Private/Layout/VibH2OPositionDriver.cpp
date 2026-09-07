#include "Layout/VibH2OPositionDriver.h"

#include "Curves/CurveFloat.h"
#include "Layout/VibH2OLayoutMath.h"
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

	const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateVortex(
		Params,
		Context.OccupiedOrdinal,
		Context.OccupiedCount,
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

void UVibH2OFlockDriver::Reseed(int32 Count, const AVibH2OStageActor* Stage)
{
	Positions.SetNum(Count);
	Velocities.SetNum(Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// A deterministic scatter inside the swimming volume, so a restart
		// gives the same school rather than a different one.
		const float A = FVibH2ONoise::UnitFloat(Index * 13 + 1) * 2.0f * PI;
		const float R = FMath::Sqrt(FVibH2ONoise::UnitFloat(Index * 13 + 2)) * BoundsRadius * 0.6f;
		const float H = (FVibH2ONoise::SignedFloat(Index * 13 + 3)) * BoundsHeight * 0.35f;

		Positions[Index] = FVector(R * FMath::Cos(A), R * FMath::Sin(A), ZOffset + H);

		const float VA = FVibH2ONoise::UnitFloat(Index * 13 + 4) * 2.0f * PI;
		Velocities[Index] = FVector(FMath::Cos(VA), FMath::Sin(VA), FVibH2ONoise::SignedFloat(Index * 13 + 5) * 0.2f)
			* FMath::Lerp(MinSpeed, MaxSpeed, 0.5f);
	}
	bSeeded = true;
}

void UVibH2OFlockDriver::PrepareFrame_Implementation(const FVibH2OFrameContext& Frame)
{
	const int32 Count = Frame.OccupiedCount;
	if (Count <= 0)
	{
		bSeeded = false;
		return;
	}

	// The school only swims while it is on stage. Integrating it behind a
	// morph that sits at zero would burn the time budget on something nobody
	// can see, and would teleport the school forward when the morph opens.
	if (Frame.BlendAlpha <= KINDA_SMALL_NUMBER)
	{
		bSeeded = false;
		return;
	}

	if (!bSeeded || Positions.Num() != Count)
	{
		Reseed(Count, Frame.Stage);
	}

	// Clamp the step: a hitch or a paused editor must not fling the school
	// across the volume in one frame.
	const float Dt = FMath::Clamp(Frame.DeltaSeconds, 0.0f, 0.05f);
	if (Dt <= 0.0f)
	{
		return;
	}

	const float Sync = FMath::Clamp(Frame.CollectiveSynchrony, 0.0f, 1.0f);

	// The dial the piece cares about. A synchronised room swims as one body;
	// a room that has drifted apart keeps separation but loses the will to
	// stay together, and the school opens out into a loose cloud.
	const float Cohesion = CohesionWeight * FMath::Lerp(0.15f, 1.0f, Sync);
	const float Alignment = AlignmentWeight * FMath::Lerp(0.10f, 1.0f, Sync);
	const float Wander = FMath::Lerp(1.0f, 0.15f, Sync);

	const float Speed = FMath::Lerp(MinSpeed, MaxSpeed,
		FMath::Clamp(0.35f + Frame.AverageExcitation * ExcitationSpeedBoost, 0.0f, 1.0f));

	// One slow attractor the whole school follows - what gives a shoal its
	// travelling direction instead of a stationary swarm.
	const float AT = Frame.TimeSeconds * AttractorSpeed * 2.0f * PI;
	const FVector Attractor(
		FMath::Cos(AT) * BoundsRadius * 0.45f,
		FMath::Sin(AT * 1.3f) * BoundsRadius * 0.45f,
		ZOffset + FMath::Sin(AT * 0.7f) * BoundsHeight * 0.22f);

	const float NeighbourSq = NeighbourRadius * NeighbourRadius;
	const float SeparationSq = SeparationRadius * SeparationRadius;

	TArray<FVector> NewVelocities;
	NewVelocities.SetNum(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Pos = Positions[i];
		FVector Separate = FVector::ZeroVector;
		FVector Align = FVector::ZeroVector;
		FVector Centre = FVector::ZeroVector;
		int32 Neighbours = 0;

		for (int32 j = 0; j < Count; ++j)
		{
			if (j == i)
			{
				continue;
			}
			const FVector Delta = Positions[j] - Pos;
			const float DistSq = Delta.SizeSquared();
			if (DistSq > NeighbourSq || DistSq < KINDA_SMALL_NUMBER)
			{
				continue;
			}

			++Neighbours;
			Align += Velocities[j];
			Centre += Positions[j];

			if (DistSq < SeparationSq)
			{
				// Push harder the closer it gets: an inverse-distance falloff
				// is what stops bubbles from ever interpenetrating.
				Separate -= Delta / DistSq * SeparationRadius;
			}
		}

		FVector Steer = FVector::ZeroVector;

		if (Neighbours > 0)
		{
			Align /= Neighbours;
			Centre /= Neighbours;
			Steer += Separate * SeparationWeight;
			Steer += (Align.GetSafeNormal() * Speed - Velocities[i]).GetSafeNormal() * Alignment * Speed;
			Steer += (Centre - Pos).GetSafeNormal() * Cohesion * Speed;
		}

		Steer += (Attractor - Pos).GetSafeNormal() * AttractorWeight * Speed;

		// A little wander of its own, so a desynchronised school looks alive
		// rather than merely spread out.
		const float WT = Frame.TimeSeconds * 0.6f;
		Steer += FVector(
			FMath::Sin(WT + i * 1.7f),
			FMath::Cos(WT * 1.1f + i * 2.3f),
			FMath::Sin(WT * 0.8f + i * 3.1f) * 0.5f) * Wander * Speed * 0.6f;

		// Containment: a soft push back once past the shell, so the school
		// turns rather than bounces.
		const FVector Flat(Pos.X, Pos.Y, 0.0f);
		const float Radial = Flat.Size();
		if (Radial > BoundsRadius)
		{
			Steer -= Flat.GetSafeNormal() * (Radial - BoundsRadius) * 4.0f;
		}
		const float DZ = Pos.Z - ZOffset;
		if (FMath::Abs(DZ) > BoundsHeight * 0.5f)
		{
			Steer.Z -= FMath::Sign(DZ) * (FMath::Abs(DZ) - BoundsHeight * 0.5f) * 4.0f;
		}

		FVector V = Velocities[i] + Steer * Dt;

		// Bounded on both sides: a fish never stops dead, and never teleports.
		const float S = V.Size();
		if (S < KINDA_SMALL_NUMBER)
		{
			V = FVector(1.0f, 0.0f, 0.0f) * MinSpeed;
		}
		else
		{
			V = V / S * FMath::Clamp(S, MinSpeed, FMath::Max(Speed, MinSpeed + 1.0f));
		}
		NewVelocities[i] = V;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		Velocities[i] = NewVelocities[i];
		Positions[i] += Velocities[i] * Dt;
	}

	LastTime = Frame.TimeSeconds;
}

FTransform UVibH2OFlockDriver::EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const
{
	const int32 Index = Context.OccupiedOrdinal;
	if (!Positions.IsValidIndex(Index))
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
