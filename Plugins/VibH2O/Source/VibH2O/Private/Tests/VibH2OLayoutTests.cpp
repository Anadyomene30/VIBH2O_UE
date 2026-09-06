// Automation tests for the geometry and the beat.
//
// These exist because three of the roadmap's criteria are stated as exact
// properties - "exact return to the straight grid", "no bubble leaves its
// cell", "BeatPhase with no discontinuity" - and an exact property is better
// verified by a test than by eye.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Layout/VibH2OLayoutMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// Pitfall 4 - the order in which the seats are walked.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSeatOrderTest,
	"VibH2O.Salle.OrdreDesSieges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSeatOrderTest::RunTest(const FString& Parameters)
{
	// A 7 x 3 room: non-square, so the transposition shows. It is the only test
	// that settles the doubt of pitfall 4.
	const int32 Columns = 7;
	const int32 Rows = 3;

	int32 Column = -1;
	int32 Row = -1;

	// ColumnMajor: index = column * rows + row + 1.
	// Indices 1, 2, 3 run down the first column; index 4 moves to the next.
	FVibH2OLayoutMath::SeatIndexToColumnRow(1, Columns, Rows, EVibH2OSeatOrder::ColumnMajor, Column, Row);
	TestEqual(TEXT("ColumnMajor index 1 -> colonne 0"), Column, 0);
	TestEqual(TEXT("ColumnMajor index 1 -> rangee 0"), Row, 0);

	FVibH2OLayoutMath::SeatIndexToColumnRow(3, Columns, Rows, EVibH2OSeatOrder::ColumnMajor, Column, Row);
	TestEqual(TEXT("ColumnMajor index 3 -> colonne 0"), Column, 0);
	TestEqual(TEXT("ColumnMajor index 3 -> rangee 2"), Row, 2);

	FVibH2OLayoutMath::SeatIndexToColumnRow(4, Columns, Rows, EVibH2OSeatOrder::ColumnMajor, Column, Row);
	TestEqual(TEXT("ColumnMajor index 4 -> colonne 1"), Column, 1);
	TestEqual(TEXT("ColumnMajor index 4 -> rangee 0"), Row, 0);

	// RowMajor: index = row * columns + column + 1.
	FVibH2OLayoutMath::SeatIndexToColumnRow(4, Columns, Rows, EVibH2OSeatOrder::RowMajor, Column, Row);
	TestEqual(TEXT("RowMajor index 4 -> colonne 3"), Column, 3);
	TestEqual(TEXT("RowMajor index 4 -> rangee 0"), Row, 0);

	// The two orders must differ on a non-square room: that is precisely what
	// makes the 7 x 3 test irreplaceable.
	int32 ColumnA = 0, RowA = 0, ColumnB = 0, RowB = 0;
	FVibH2OLayoutMath::SeatIndexToColumnRow(2, Columns, Rows, EVibH2OSeatOrder::ColumnMajor, ColumnA, RowA);
	FVibH2OLayoutMath::SeatIndexToColumnRow(2, Columns, Rows, EVibH2OSeatOrder::RowMajor, ColumnB, RowB);
	TestTrue(TEXT("Les deux ordres donnent bien des places differentes"), ColumnA != ColumnB || RowA != RowB);

	// Round trip over the whole room, in both orders.
	for (int32 OrderIndex = 0; OrderIndex < 2; ++OrderIndex)
	{
		const EVibH2OSeatOrder Order = (OrderIndex == 0) ? EVibH2OSeatOrder::ColumnMajor : EVibH2OSeatOrder::RowMajor;
		TArray<int32> Visited;
		Visited.Init(0, Columns * Rows);

		for (int32 Index = 1; Index <= Columns * Rows; ++Index)
		{
			if (!FVibH2OLayoutMath::SeatIndexToColumnRow(Index, Columns, Rows, Order, Column, Row))
			{
				AddError(FString::Printf(TEXT("L'index %d aurait du etre valide."), Index));
				return false;
			}
			TestTrue(TEXT("Colonne dans les bornes"), Column >= 0 && Column < Columns);
			TestTrue(TEXT("Rangee dans les bornes"), Row >= 0 && Row < Rows);

			const int32 Back = FVibH2OLayoutMath::ColumnRowToSeatIndex(Column, Row, Columns, Rows, Order);
			TestEqual(TEXT("Aller-retour index -> place -> index"), Back, Index);

			Visited[Row * Columns + Column] += 1;
		}

		// Every cell must be reached exactly once: the mapping is a bijection,
		// not an approximation.
		for (int32 Cell = 0; Cell < Visited.Num(); ++Cell)
		{
			if (Visited[Cell] != 1)
			{
				AddError(FString::Printf(TEXT("La place %d a ete atteinte %d fois."), Cell, Visited[Cell]));
				return false;
			}
		}
	}

	// Out-of-range indices.
	TestFalse(TEXT("Index 0 refuse"), FVibH2OLayoutMath::SeatIndexToColumnRow(0, Columns, Rows, EVibH2OSeatOrder::ColumnMajor, Column, Row));
	TestFalse(TEXT("Index au dela de la salle refuse"), FVibH2OLayoutMath::SeatIndexToColumnRow(22, Columns, Rows, EVibH2OSeatOrder::ColumnMajor, Column, Row));
	TestFalse(TEXT("Salle vide refusee"), FVibH2OLayoutMath::SeatIndexToColumnRow(1, 0, 0, EVibH2OSeatOrder::ColumnMajor, Column, Row));

	return true;
}

// ---------------------------------------------------------------------------
// Curvature: EXACT return to the straight grid at zero.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OCurvatureTest,
	"VibH2O.Salle.Courbure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OCurvatureTest::RunTest(const FString& Parameters)
{
	FVibH2OGridParams Params;
	Params.Columns = 7;
	Params.Rows = 3;
	Params.ColumnSpacing = 120.0f;
	Params.RowSpacing = 140.0f;
	Params.CurvatureAngleDeg = 0.0f;
	Params.ElevationPerRow = 0.0f;
	Params.DepthAmplitude = 0.0f;

	// At zero angle the position must equal the straight grid exactly. Not
	// "within epsilon": exactly.
	for (int32 Row = 0; Row < Params.Rows; ++Row)
	{
		for (int32 Column = 0; Column < Params.Columns; ++Column)
		{
			const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateGrid(Params, Column, Row);

			const float ExpectedX = (static_cast<float>(Row) - 0.5f * (Params.Rows - 1)) * Params.RowSpacing;
			const float ExpectedY = (static_cast<float>(Column) - 0.5f * (Params.Columns - 1)) * Params.ColumnSpacing;

			if (Layout.Location.X != ExpectedX || Layout.Location.Y != ExpectedY)
			{
				AddError(FString::Printf(
					TEXT("Salle droite inexacte en (%d,%d) : obtenu (%f, %f), attendu (%f, %f)."),
					Column, Row, Layout.Location.X, Layout.Location.Y, ExpectedX, ExpectedY));
				return false;
			}
			if (!Layout.Rotation.IsNearlyZero())
			{
				AddError(TEXT("Une salle droite ne doit produire aucune rotation."));
				return false;
			}
		}
	}
	AddInfo(TEXT("A CurvatureAngle = 0, la grille est exacte au bit pres."));

	// Rake: with no curve, Z is exactly Row * ElevationPerRow.
	Params.ElevationPerRow = 35.0f;
	for (int32 Row = 0; Row < Params.Rows; ++Row)
	{
		const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateGrid(Params, 0, Row);
		TestEqual(TEXT("Gradins exacts"), static_cast<float>(Layout.Location.Z), static_cast<float>(Row) * 35.0f);
	}
	Params.ElevationPerRow = 0.0f;

	// Continuity: sweeping the angle must produce no jump. Were the switch to
	// the straight formula badly written, it would show here.
	const int32 Steps = 4000;
	FVector Previous = FVector::ZeroVector;
	double MaxStep = 0.0;
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		Params.CurvatureAngleDeg = FMath::Lerp(-12.0f, 12.0f, static_cast<float>(Step) / Steps);
		const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateGrid(Params, Params.Columns - 1, Params.Rows - 1);
		if (Step > 0)
		{
			MaxStep = FMath::Max(MaxStep, (Layout.Location - Previous).Size());
		}
		Previous = Layout.Location;
	}
	// 24 degrees swept in 4000 steps on a corner seat: every step must stay
	// tiny. A jump would betray a discontinuity.
	TestTrue(FString::Printf(TEXT("Balayage continu (plus grand pas : %f cm)"), MaxStep), MaxStep < 2.0);

	// Symmetry: two opposite angles must give mirrored rooms.
	Params.CurvatureAngleDeg = 8.0f;
	const FVibH2OSeatLayout Right = FVibH2OLayoutMath::EvaluateGrid(Params, Params.Columns - 1, 1);
	Params.CurvatureAngleDeg = -8.0f;
	const FVibH2OSeatLayout Left = FVibH2OLayoutMath::EvaluateGrid(Params, Params.Columns - 1, 1);

	TestTrue(TEXT("Miroir : meme ecart lateral"), FMath::IsNearlyEqual(Right.Location.Y, Left.Location.Y, 0.01));
	TestTrue(TEXT("Miroir : profondeur opposee"), FMath::IsNearlyEqual(Right.Location.X, -Left.Location.X, 0.01));
	TestTrue(TEXT("Miroir : yaw oppose"), FMath::IsNearlyEqual(Right.Rotation.Yaw, -Left.Rotation.Yaw, 0.01));

	// Constant spacing: on a curved room, the distance between neighbours in
	// the last row must stay ColumnSpacing.
	Params.CurvatureAngleDeg = 8.0f;
	Params.bFanOut = false;
	const FVibH2OSeatLayout A = FVibH2OLayoutMath::EvaluateGrid(Params, 0, Params.Rows - 1);
	const FVibH2OSeatLayout B = FVibH2OLayoutMath::EvaluateGrid(Params, 1, Params.Rows - 1);
	const double Spacing = (A.Location - B.Location).Size();
	TestTrue(
		FString::Printf(TEXT("Espacement constant conserve au fond (%f cm attendu ~120)"), Spacing),
		FMath::Abs(Spacing - Params.ColumnSpacing) < 3.0);

	// In fan-out mode the same measurement must widen instead.
	Params.bFanOut = true;
	const FVibH2OSeatLayout C = FVibH2OLayoutMath::EvaluateGrid(Params, 0, Params.Rows - 1);
	const FVibH2OSeatLayout D = FVibH2OLayoutMath::EvaluateGrid(Params, 1, Params.Rows - 1);
	const double FanSpacing = (C.Location - D.Location).Size();
	TestTrue(
		FString::Printf(TEXT("En eventail, le fond s'elargit (%f > %f)"), FanSpacing, Spacing),
		FanSpacing > Spacing + 1.0);

	return true;
}

// ---------------------------------------------------------------------------
// Depth noise: deterministic and smooth, not random.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ONoiseTest,
	"VibH2O.Salle.BruitDeProfondeur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ONoiseTest::RunTest(const FString& Parameters)
{
	// Deterministic: the relief must be the same from one launch to the next.
	TestEqual(TEXT("Deterministe"), FVibH2ONoise::Value2D(3.25f, 1.75f), FVibH2ONoise::Value2D(3.25f, 1.75f));

	// Bounded.
	for (int32 Index = 0; Index < 500; ++Index)
	{
		const float V = FVibH2ONoise::Value2D(Index * 0.137f, Index * 0.311f);
		if (V < -1.0f || V > 1.0f)
		{
			AddError(FString::Printf(TEXT("Bruit hors bornes : %f"), V));
			return false;
		}
	}

	// Smooth. This is the property that matters: pure randomness would
	// decorrelate neighbours and make the rows unreadable. At a scale of 0.25
	// per seat, two neighbours must stay close.
	const float Scale = 0.25f;
	double MaxNeighbourDelta = 0.0;
	for (int32 Row = 0; Row < 12; ++Row)
	{
		for (int32 Column = 0; Column < 24; ++Column)
		{
			const float Here = FVibH2ONoise::Value2D(Column * Scale, Row * Scale);
			const float Next = FVibH2ONoise::Value2D((Column + 1) * Scale, Row * Scale);
			MaxNeighbourDelta = FMath::Max(MaxNeighbourDelta, FMath::Abs(Here - Next));
		}
	}
	TestTrue(
		FString::Printf(TEXT("Le bruit est lisse entre voisins (ecart max %f)"), MaxNeighbourDelta),
		MaxNeighbourDelta < 0.75);

	return true;
}

// ---------------------------------------------------------------------------
// Drift: bounded by construction.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ODriftTest,
	"VibH2O.Mouvement.Derive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ODriftTest::RunTest(const FString& Parameters)
{
	FVibH2ODriftParams Params;
	Params.Threshold = 0.4f;
	// Deliberately absurd intensity: the clamp must hold regardless.
	Params.Intensity = 100000.0f;
	Params.MaxRatio = 0.35f;

	const float MinSpacing = 120.0f;
	const float MaxAllowed = MinSpacing * Params.MaxRatio;

	// Below the threshold: complete stillness, not a residue.
	for (float Excitation = 0.0f; Excitation <= Params.Threshold; Excitation += 0.05f)
	{
		for (int32 Seed = 0; Seed < 50; ++Seed)
		{
			const FVector Offset = FVibH2OLayoutMath::EvaluateDriftOffset(Params, Seed, Seed * 0.7f, Excitation, MinSpacing);
			if (!Offset.IsZero())
			{
				AddError(FString::Printf(TEXT("Derive non nulle a excitation %f, sous le seuil."), Excitation));
				return false;
			}
		}
	}

	// Above the threshold: never past the bound, whatever the instant, the seed
	// and the arousal.
	double MaxObserved = 0.0;
	for (int32 Seed = 0; Seed < 200; ++Seed)
	{
		for (int32 Step = 0; Step < 120; ++Step)
		{
			const float Time = Step * 0.25f;
			const float Excitation = 0.4f + (Step % 13) * 0.05f;
			const FVector Offset = FVibH2OLayoutMath::EvaluateDriftOffset(Params, Seed, Time, Excitation, MinSpacing);
			MaxObserved = FMath::Max(MaxObserved, Offset.Size());
		}
	}

	TestTrue(
		FString::Printf(TEXT("Derive ecretee a %f cm (limite %f)"), MaxObserved, MaxAllowed),
		MaxObserved <= MaxAllowed + 0.01);

	// The bound is strictly under half a cell, so the bubble mathematically
	// cannot reach its neighbour.
	TestTrue(TEXT("La borne reste sous la demi-case"), MaxAllowed < MinSpacing * 0.5f);

	// Continuity of onset: just above the threshold the drift must grow from
	// zero, not appear all at once.
	const float JustAbove = FVibH2OLayoutMath::ComputeDriftAmount(Params.Threshold + 0.001f, Params.Threshold);
	TestTrue(TEXT("Le declenchement est progressif"), JustAbove > 0.0f && JustAbove < 0.01f);
	TestEqual(TEXT("Amplitude maximale a excitation 1"), FVibH2OLayoutMath::ComputeDriftAmount(1.0f, Params.Threshold), 1.0f);

	return true;
}

// ---------------------------------------------------------------------------
// Beat: the phase never jumps.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OBeatPhaseTest,
	"VibH2O.Battement.ContinuiteDeLaPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OBeatPhaseTest::RunTest(const FString& Parameters)
{
	const float DeltaTime = 1.0f / 60.0f;

	// Counting. The duration is DELIBERATELY chosen off the boundary: exactly
	// 10 seconds at 60 BPM would land precisely on the tenth beat, and in
	// floating point 600 additions of 1/60 do not land exactly on 10.0 - the
	// crossing may fall either side. Asserting an exact equality on a boundary
	// tests floating point, not the code.
	float Phase = 0.0f;
	int32 TotalBeats = 0;
	for (int32 Frame = 0; Frame < 630; ++Frame) // 10,5 s
	{
		int32 Beats = 0;
		Phase = FVibH2OLayoutMath::AdvanceBeatPhase(Phase, 60.0f, DeltaTime, Beats);
		TotalBeats += Beats;
	}
	TestEqual(TEXT("60 BPM pendant 10,5 s donne 10 battements"), TotalBeats, 10);

	// The property that really matters: the heart must neither run ahead nor
	// fall behind over the length of a performance.
	//
	// BeatCount + Phase is the number of beats elapsed since the start, as a
	// continuous quantity. It must track Time * BPM / 60. Since the phase
	// decrements by 1 on each beat and stays within [0,1[, the rounding error
	// does not accumulate: it bounds itself.
	Phase = 0.0f;
	TotalBeats = 0;
	const int32 LongRunFrames = 3600; // une minute a 60 images par seconde
	for (int32 Frame = 0; Frame < LongRunFrames; ++Frame)
	{
		int32 Beats = 0;
		Phase = FVibH2OLayoutMath::AdvanceBeatPhase(Phase, 72.0f, DeltaTime, Beats);
		TotalBeats += Beats;
	}
	const double ElapsedBeats = static_cast<double>(TotalBeats) + Phase;
	const double ExpectedBeats = static_cast<double>(LongRunFrames) * DeltaTime * (72.0 / 60.0);
	const double DriftBeats = FMath::Abs(ElapsedBeats - ExpectedBeats);

	TestTrue(
		FString::Printf(TEXT("Aucune derive sur une minute a 72 BPM (ecart %g battement)"), DriftBeats),
		DriftBeats < 0.01);

	// THE roadmap test: vary the BPM abruptly and check that the phase advances
	// with no discontinuity.
	//
	// Continuity is defined precisely here: on every frame the phase advances
	// by exactly DeltaTime * BPM / 60, modulo 1. It is never rewritten. That is
	// what a timer restarted on each new BPM value could not guarantee, and
	// what keeps the material's rings from glitching.
	Phase = 0.0f;
	double MaxError = 0.0;
	float PreviousPhase = 0.0f;

	const float BpmSequence[] = { 60.0f, 180.0f, 45.0f, 200.0f, 55.0f, 120.0f };

	for (int32 SegmentIndex = 0; SegmentIndex < UE_ARRAY_COUNT(BpmSequence); ++SegmentIndex)
	{
		const float Bpm = BpmSequence[SegmentIndex];
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			PreviousPhase = Phase;

			int32 Beats = 0;
			Phase = FVibH2OLayoutMath::AdvanceBeatPhase(Phase, Bpm, DeltaTime, Beats);

			// Expected advance, brought back into [0,1[.
			const float Expected = FMath::Frac(PreviousPhase + DeltaTime * (Bpm / 60.0f));
			MaxError = FMath::Max<double>(MaxError, FMath::Abs(Phase - Expected));
		}
	}

	TestTrue(
		FString::Printf(TEXT("Phase continue a travers six changements brutaux de BPM (ecart max %g)"), MaxError),
		MaxError < 1.0e-5);

	// The phase always stays within [0,1[.
	TestTrue(TEXT("Phase bornee"), Phase >= 0.0f && Phase < 1.0f);

	// An abnormally long frame must not lose beats.
	Phase = 0.0f;
	int32 Beats = 0;
	Phase = FVibH2OLayoutMath::AdvanceBeatPhase(Phase, 120.0f, 2.0f, Beats);
	TestEqual(TEXT("Une frame de 2 s a 120 BPM compte 4 battements"), Beats, 4);
	TestTrue(TEXT("Phase toujours bornee apres une frame longue"), Phase >= 0.0f && Phase < 1.0f);

	// Degenerate cases: neither crash nor NaN.
	int32 Ignored = 0;
	TestEqual(TEXT("BPM nul laisse la phase en place"), FVibH2OLayoutMath::AdvanceBeatPhase(0.25f, 0.0f, DeltaTime, Ignored), 0.25f);
	TestEqual(TEXT("BPM negatif laisse la phase en place"), FVibH2OLayoutMath::AdvanceBeatPhase(0.25f, -60.0f, DeltaTime, Ignored), 0.25f);
	TestEqual(TEXT("DeltaTime nul laisse la phase en place"), FVibH2OLayoutMath::AdvanceBeatPhase(0.25f, 60.0f, 0.0f, Ignored), 0.25f);

	const float FromNaN = FVibH2OLayoutMath::AdvanceBeatPhase(FMath::Sqrt(-1.0f), 60.0f, DeltaTime, Ignored);
	TestTrue(TEXT("Une phase NaN est rattrapee"), FMath::IsFinite(FromNaN));

	return true;
}

// ---------------------------------------------------------------------------
// Float: distinct phases, not a unison.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OFloatTest,
	"VibH2O.Mouvement.Flottement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OFloatTest::RunTest(const FString& Parameters)
{
	FVibH2OFloatParams Params;
	Params.Amplitude = 12.0f;
	Params.Speed = 0.35f;

	// Bounded: the offset never exceeds the amplitude on any axis.
	for (int32 Seed = 0; Seed < 200; ++Seed)
	{
		for (int32 Step = 0; Step < 40; ++Step)
		{
			const FVector Offset = FVibH2OLayoutMath::EvaluateFloatOffset(Params, Seed, Step * 0.37f);
			if (FMath::Abs(Offset.X) > Params.Amplitude + 0.001
				|| FMath::Abs(Offset.Y) > Params.Amplitude + 0.001
				|| FMath::Abs(Offset.Z) > Params.Amplitude + 0.001)
			{
				AddError(TEXT("Le flottement depasse son amplitude."));
				return false;
			}
		}
	}

	// The critical point: two neighbouring bubbles must not undulate in unison.
	// We measure the spread of positions at a given instant; were every phase
	// identical it would be zero.
	const float Time = 3.7f;
	double SumZ = 0.0;
	double SumZ2 = 0.0;
	const int32 Count = 176;
	for (int32 Seed = 0; Seed < Count; ++Seed)
	{
		const double Z = FVibH2OLayoutMath::EvaluateFloatOffset(Params, Seed, Time).Z;
		SumZ += Z;
		SumZ2 += Z * Z;
	}
	const double Mean = SumZ / Count;
	const double Variance = SumZ2 / Count - Mean * Mean;
	const double StdDev = FMath::Sqrt(FMath::Max(Variance, 0.0));

	TestTrue(
		FString::Printf(TEXT("Les 176 bulles ont des phases distinctes (ecart-type %f cm)"), StdDev),
		StdDev > Params.Amplitude * 0.3);

	// Deterministic: the same bubble at the same instant gives the same thing.
	TestEqual(
		TEXT("Deterministe"),
		FVibH2OLayoutMath::EvaluateFloatOffset(Params, 42, 1.5f),
		FVibH2OLayoutMath::EvaluateFloatOffset(Params, 42, 1.5f));

	return true;
}

// ---------------------------------------------------------------------------
// Vortex: the cone opens up, and synchrony sets its order.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OVortexTest,
	"VibH2O.Tableau2.Vortex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OVortexTest::RunTest(const FString& Parameters)
{
	FVibH2OVortexParams Params;
	Params.Height = 1200.0f;
	Params.BaseRadius = 200.0f;
	Params.TopRadius = 900.0f;
	Params.SpinSpeed = 0.0f;

	const int32 Count = 100;

	// Perfect synchrony: no scatter at all, hence an exact cone.
	for (int32 Ordinal = 0; Ordinal < Count; ++Ordinal)
	{
		const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateVortex(Params, Ordinal, Count, Ordinal, 0.0f, 1.0f, 1.0f);
		const float T = static_cast<float>(Ordinal) / (Count - 1);
		const double Radius = FVector2D(Layout.Location.X, Layout.Location.Y).Size();
		const double ExpectedRadius = FMath::Lerp(Params.BaseRadius, Params.TopRadius, T);

		if (FMath::Abs(Radius - ExpectedRadius) > 0.5)
		{
			AddError(FString::Printf(TEXT("Rayon inattendu au rang %d : %f au lieu de %f."), Ordinal, Radius, ExpectedRadius));
			return false;
		}
		TestTrue(TEXT("Hauteur croissante et bornee"), Layout.Location.Z >= -0.01 && Layout.Location.Z <= Params.Height + 0.01);
	}
	AddInfo(TEXT("A synchronie 1, le vortex est un cone exact."));

	// The cone does open upward.
	const FVibH2OSeatLayout Bottom = FVibH2OLayoutMath::EvaluateVortex(Params, 0, Count, 0, 0.0f, 1.0f, 1.0f);
	const FVibH2OSeatLayout Top = FVibH2OLayoutMath::EvaluateVortex(Params, Count - 1, Count, Count - 1, 0.0f, 1.0f, 1.0f);
	TestTrue(TEXT("Le cone s'evase vers le haut"),
		FVector2D(Top.Location.X, Top.Location.Y).Size() > FVector2D(Bottom.Location.X, Bottom.Location.Y).Size());

	// Desynchronisation: the scatter must increase.
	auto MeasureSpread = [&Params, Count](float Synchrony) -> double
	{
		double Sum = 0.0;
		for (int32 Ordinal = 0; Ordinal < Count; ++Ordinal)
		{
			const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateVortex(Params, Ordinal, Count, Ordinal, 0.0f, Synchrony, Synchrony);
			const float T = static_cast<float>(Ordinal) / (Count - 1);
			const double Ideal = FMath::Lerp(Params.BaseRadius, Params.TopRadius, T);
			Sum += FMath::Abs(FVector2D(Layout.Location.X, Layout.Location.Y).Size() - Ideal);
		}
		return Sum / Count;
	};

	const double Ordered = MeasureSpread(1.0f);
	const double Scattered = MeasureSpread(0.0f);

	TestTrue(FString::Printf(TEXT("Public synchrone : spirale serree (ecart moyen %f cm)"), Ordered), Ordered < 1.0);
	TestTrue(FString::Printf(TEXT("Public decroche : dispersion (ecart moyen %f cm)"), Scattered), Scattered > 20.0);

	// A room with a single bubble must not divide by zero.
	const FVibH2OSeatLayout Single = FVibH2OLayoutMath::EvaluateVortex(Params, 0, 1, 0, 0.0f, 1.0f, 1.0f);
	TestTrue(TEXT("Une seule bulle : position finie"), Single.Location.ContainsNaN() == false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
