// Automation tests for the room's mood.
//
// The brief has three properties a test can state exactly: the three moods are
// told apart by the thresholds, a room hovering at a threshold does NOT
// flicker, and a change of mood is a cross-fade of the length asked for, never
// a cut. They are checked here on bare numbers, without a world.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "VibH2OMood.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FVibH2OMoodTrackerSettings MakeSettings()
	{
		FVibH2OMoodTrackerSettings Settings;
		Settings.CalmThreshold = 0.25f;
		Settings.ExcitedThreshold = 0.55f;
		Settings.Hysteresis = 0.04f;
		Settings.DetectionSmoothingSeconds = 1.5f;
		Settings.TransitionSeconds = 4.0f;
		return Settings;
	}

	float WeightSum(const FVibH2OMoodWeights& W)
	{
		return W.Calm + W.Normal + W.Excited;
	}

	/** Runs the tracker on a constant value for a while, at 60 frames a second. */
	int32 Run(FVibH2OMoodTracker& Tracker, float Value, float Seconds, const FVibH2OMoodTrackerSettings& Settings)
	{
		int32 Changes = 0;
		const float Dt = 1.0f / 60.0f;
		for (float T = 0.0f; T < Seconds; T += Dt)
		{
			Changes += Tracker.Update(Value, true, Dt, Settings) ? 1 : 0;
		}
		return Changes;
	}
}

// ---------------------------------------------------------------------------
// The three moods, and the dead band between them.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OMoodClassifyTest,
	"VibH2O.Ambiance.TroisEtats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OMoodClassifyTest::RunTest(const FString& Parameters)
{
	const FVibH2OMoodTrackerSettings S = MakeSettings();

	TestEqual(TEXT("Salle a 0,05 : calme"), FVibH2OMoodTracker::Classify(0.05f, EVibH2OMood::Normal, S), EVibH2OMood::Calm);
	TestEqual(TEXT("Salle a 0,40 : normale"), FVibH2OMoodTracker::Classify(0.40f, EVibH2OMood::Normal, S), EVibH2OMood::Normal);
	TestEqual(TEXT("Salle a 0,85 : excitee"), FVibH2OMoodTracker::Classify(0.85f, EVibH2OMood::Normal, S), EVibH2OMood::Excited);

	// Inside the dead band, each mood keeps itself.
	TestEqual(TEXT("0,27 depuis calme : reste calme"), FVibH2OMoodTracker::Classify(0.27f, EVibH2OMood::Calm, S), EVibH2OMood::Calm);
	TestEqual(TEXT("0,23 depuis normal : reste normal"), FVibH2OMoodTracker::Classify(0.23f, EVibH2OMood::Normal, S), EVibH2OMood::Normal);
	TestEqual(TEXT("0,53 depuis excite : reste excite"), FVibH2OMoodTracker::Classify(0.53f, EVibH2OMood::Excited, S), EVibH2OMood::Excited);
	TestEqual(TEXT("0,57 depuis normal : reste normal"), FVibH2OMoodTracker::Classify(0.57f, EVibH2OMood::Normal, S), EVibH2OMood::Normal);

	// A big jump goes straight across, without stopping at normal.
	TestEqual(TEXT("Calme vers 0,9 : directement excite"), FVibH2OMoodTracker::Classify(0.9f, EVibH2OMood::Calm, S), EVibH2OMood::Excited);
	TestEqual(TEXT("Excite vers 0,1 : directement calme"), FVibH2OMoodTracker::Classify(0.1f, EVibH2OMood::Excited, S), EVibH2OMood::Calm);

	// Thresholds typed the wrong way round still mean something.
	FVibH2OMoodTrackerSettings Swapped = S;
	Swapped.CalmThreshold = 0.55f;
	Swapped.ExcitedThreshold = 0.25f;
	TestEqual(TEXT("Seuils inverses : 0,05 reste calme"), FVibH2OMoodTracker::Classify(0.05f, EVibH2OMood::Normal, Swapped), EVibH2OMood::Calm);
	TestEqual(TEXT("Seuils inverses : 0,85 reste excite"), FVibH2OMoodTracker::Classify(0.85f, EVibH2OMood::Normal, Swapped), EVibH2OMood::Excited);
	return true;
}

// ---------------------------------------------------------------------------
// No flicker at a threshold.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OMoodNoFlickerTest,
	"VibH2O.Ambiance.PasDeClignotement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OMoodNoFlickerTest::RunTest(const FString& Parameters)
{
	const FVibH2OMoodTrackerSettings S = MakeSettings();

	// A room sitting right on the excited threshold, with frame-to-frame noise
	// well beyond the hysteresis margin - the worst case for flicker.
	FVibH2OMoodTracker Tracker;
	Tracker.Snap(0.40f, S);
	int32 Changes = 0;
	const float Dt = 1.0f / 60.0f;
	for (int32 Frame = 0; Frame < 60 * 60; ++Frame)
	{
		const float Noise = ((Frame * 7919) % 101) / 100.0f - 0.5f; // -0.5 .. 0.5
		const float Value = S.ExcitedThreshold + Noise * 0.16f;
		Changes += Tracker.Update(Value, true, Dt, S) ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("Une minute a cheval sur le seuil, bruit de +/-0,08 : %d changement(s) d'etat, au plus 1"), Changes),
		Changes <= 1);

	// A slow, steady rise crosses each threshold exactly once.
	FVibH2OMoodTracker Rising;
	Rising.Snap(0.0f, S);
	int32 RisingChanges = 0;
	for (int32 Frame = 0; Frame <= 60 * 30; ++Frame)
	{
		RisingChanges += Rising.Update(Frame / (60.0f * 30.0f), true, Dt, S) ? 1 : 0;
	}
	TestEqual(TEXT("Une montee lente de 0 a 1 : calme, normal, excite, deux changements"), RisingChanges, 2);
	TestEqual(TEXT("La montee finit excitee"), Rising.GetMood(), EVibH2OMood::Excited);

	// No data, no verdict: an empty room must not slide to calm.
	FVibH2OMoodTracker Held;
	Held.Snap(0.85f, S);
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		Held.Update(0.0f, false, Dt, S);
	}
	TestEqual(TEXT("Sans capteur actif, l'etat est tenu"), Held.GetMood(), EVibH2OMood::Excited);
	return true;
}

// ---------------------------------------------------------------------------
// A change of mood is a cross-fade, of the length asked for.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OMoodCrossFadeTest,
	"VibH2O.Ambiance.FonduEntreEtats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OMoodCrossFadeTest::RunTest(const FString& Parameters)
{
	FVibH2OMoodTrackerSettings S = MakeSettings();
	// Detection without smoothing, so the fade alone is timed.
	S.DetectionSmoothingSeconds = 0.0f;

	FVibH2OMoodTracker Tracker;
	Tracker.Snap(0.05f, S);
	TestEqual(TEXT("Depart calme"), Tracker.GetMood(), EVibH2OMood::Calm);

	const float Dt = 1.0f / 60.0f;
	float MaxStep = 0.0f;
	float PreviousCalm = Tracker.GetWeights().Calm;
	float WorstSum = 0.0f;
	float HalfwayExcited = -1.0f;
	for (int32 Frame = 1; Frame <= 60 * 5; ++Frame)
	{
		Tracker.Update(0.9f, true, Dt, S);
		const FVibH2OMoodWeights& W = Tracker.GetWeights();
		MaxStep = FMath::Max(MaxStep, FMath::Abs(W.Calm - PreviousCalm));
		PreviousCalm = W.Calm;
		WorstSum = FMath::Max(WorstSum, FMath::Abs(WeightSum(W) - 1.0f));
		if (Frame == 120)
		{
			HalfwayExcited = W.Excited;
		}
		if (Frame == 60 * 3)
		{
			TestTrue(TEXT("A 3 s sur 4, le fondu n'est pas fini"), Tracker.IsTransitioning());
		}
	}

	TestEqual(TEXT("Arrivee excitee"), Tracker.GetMood(), EVibH2OMood::Excited);
	TestFalse(TEXT("A 5 s, le fondu de 4 s est fini"), Tracker.IsTransitioning());
	TestNearlyEqual(TEXT("Poids final de l'etat excite"), Tracker.GetWeights().Excited, 1.0f, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("A mi-fondu, moitie excite"), HalfwayExcited, 0.5f, 0.02f);
	TestTrue(TEXT("Les trois poids somment toujours a 1"), WorstSum < 1.0e-4f);
	// A linear 4 s fade at 60 fps moves 0.0042 a frame; smoothstep peaks at
	// 1.5 times that. Anything much above is a cut.
	TestTrue(FString::Printf(TEXT("Aucun saut : plus grand pas %.4f par image"), MaxStep), MaxStep < 0.008f);

	// Changing one's mind half way turns round from where the blend stands.
	FVibH2OMoodTracker Turning;
	Turning.Snap(0.05f, S);
	Run(Turning, 0.9f, 2.0f, S);
	const float CalmBefore = Turning.GetWeights().Calm;
	Turning.Update(0.05f, true, Dt, S);
	TestTrue(TEXT("Demi-tour a mi-fondu : pas de retour brutal"), FMath::Abs(Turning.GetWeights().Calm - CalmBefore) < 0.01f);
	TestEqual(TEXT("Demi-tour : l'etat vise redevient calme"), Turning.GetMood(), EVibH2OMood::Calm);

	// Forcing the mood wins over the room.
	FVibH2OMoodTrackerSettings Forced = S;
	Forced.Override = EVibH2OMoodOverride::Calm;
	FVibH2OMoodTracker ForcedTracker;
	ForcedTracker.Snap(0.9f, Forced);
	TestEqual(TEXT("Force calme sur une salle excitee : calme"), ForcedTracker.GetMood(), EVibH2OMood::Calm);
	return true;
}

// ---------------------------------------------------------------------------
// Palettes: each mood shows its own colours, individuals spread within them.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OMoodPaletteTest,
	"VibH2O.Ambiance.Palettes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OMoodPaletteTest::RunTest(const FString& Parameters)
{
	const FVibH2OMoodPalette Palette;

	const FVibH2OMoodLook Calm = Palette.Blend(FVibH2OMoodWeights::For(EVibH2OMood::Calm));
	const FVibH2OMoodLook Excited = Palette.Blend(FVibH2OMoodWeights::For(EVibH2OMood::Excited));
	TestTrue(TEXT("Calme pur : la couleur de la palette calme"), Calm.ColorLow.Equals(Palette.Calm.ColorLow));
	TestTrue(TEXT("Excite pur : la couleur de la palette excitee"), Excited.ColorHigh.Equals(Palette.Excited.ColorHigh));

	// Cold to warm, by default: the calm palette is bluer than red, the
	// excited one redder than blue.
	const FLinearColor CalmMid = Calm.ColorAt(0.5f);
	const FLinearColor ExcitedMid = Excited.ColorAt(0.5f);
	TestTrue(TEXT("Palette calme par defaut : froide"), CalmMid.B > CalmMid.R);
	TestTrue(TEXT("Palette excitee par defaut : chaude"), ExcitedMid.R > ExcitedMid.B);

	// Energy rises with the mood.
	TestTrue(TEXT("Stries plus rapides en excite qu'en calme"), Excited.StripeSpeed > Calm.StripeSpeed);
	TestTrue(TEXT("Plus de lumiere en excite qu'en calme"), Excited.Glow > Calm.Glow);
	TestTrue(TEXT("Bulles plus grandes en excite qu'en calme"), Excited.SizeScale > Calm.SizeScale);
	TestTrue(TEXT("Flottement plus ample en excite qu'en calme"), Excited.FloatScale > Calm.FloatScale);

	// Half-way blend sits half-way.
	FVibH2OMoodWeights Half;
	Half.Calm = 0.5f;
	Half.Normal = 0.0f;
	Half.Excited = 0.5f;
	const FVibH2OMoodLook Mixed = Palette.Blend(Half);
	TestNearlyEqual(TEXT("Mi-chemin : vitesse des stries moyenne"), Mixed.StripeSpeed,
		0.5f * (Palette.Calm.StripeSpeed + Palette.Excited.StripeSpeed), KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("Alpha de couleur toujours a 1"), Mixed.ColorAt(0.3f).A, 1.0f, KINDA_SMALL_NUMBER);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
