#include "Layout/VibH2OPositionDriver.h"

#include "Curves/CurveFloat.h"
#include "Layout/VibH2OLayoutMath.h"
#include "VibH2OStageActor.h"

// --------------------------------------------------------------------- Base

FTransform UVibH2OPositionDriver::EvaluateSeat_Implementation(const FVibH2OLayoutContext& Context) const
{
	return FTransform::Identity;
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
