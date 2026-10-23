#include "VibH2OMood.h"

// ------------------------------------------------------------------ Weights

FVibH2OMoodWeights FVibH2OMoodWeights::For(EVibH2OMood Mood)
{
	FVibH2OMoodWeights W;
	W.Calm = (Mood == EVibH2OMood::Calm) ? 1.0f : 0.0f;
	W.Normal = (Mood == EVibH2OMood::Normal) ? 1.0f : 0.0f;
	W.Excited = (Mood == EVibH2OMood::Excited) ? 1.0f : 0.0f;
	return W;
}

FVibH2OMoodWeights FVibH2OMoodWeights::Lerp(const FVibH2OMoodWeights& A, const FVibH2OMoodWeights& B, float Alpha)
{
	// A convex combination of two weight sets that each sum to one also sums
	// to one: no renormalisation needed, and none of the three ever goes
	// negative.
	FVibH2OMoodWeights W;
	W.Calm = FMath::Lerp(A.Calm, B.Calm, Alpha);
	W.Normal = FMath::Lerp(A.Normal, B.Normal, Alpha);
	W.Excited = FMath::Lerp(A.Excited, B.Excited, Alpha);
	return W;
}

// --------------------------------------------------------------------- Look

FVibH2OMoodLook FVibH2OMoodLook::Blend(const FVibH2OMoodLook& Calm, const FVibH2OMoodLook& Normal, const FVibH2OMoodLook& Excited,
	const FVibH2OMoodWeights& W)
{
	FVibH2OMoodLook Out;
	auto Mix = [&W](float C, float N, float E) { return C * W.Calm + N * W.Normal + E * W.Excited; };

	Out.ColorLow = Calm.ColorLow * W.Calm + Normal.ColorLow * W.Normal + Excited.ColorLow * W.Excited;
	Out.ColorHigh = Calm.ColorHigh * W.Calm + Normal.ColorHigh * W.Normal + Excited.ColorHigh * W.Excited;
	Out.ColorLow.A = 1.0f;
	Out.ColorHigh.A = 1.0f;
	Out.Glow = Mix(Calm.Glow, Normal.Glow, Excited.Glow);
	Out.StripeSpeed = Mix(Calm.StripeSpeed, Normal.StripeSpeed, Excited.StripeSpeed);
	Out.Turbulence = Mix(Calm.Turbulence, Normal.Turbulence, Excited.Turbulence);
	Out.FloatScale = Mix(Calm.FloatScale, Normal.FloatScale, Excited.FloatScale);
	Out.FloatSpeedScale = Mix(Calm.FloatSpeedScale, Normal.FloatSpeedScale, Excited.FloatSpeedScale);
	Out.DriftScale = Mix(Calm.DriftScale, Normal.DriftScale, Excited.DriftScale);
	Out.SizeScale = Mix(Calm.SizeScale, Normal.SizeScale, Excited.SizeScale);
	Out.PulseScale = Mix(Calm.PulseScale, Normal.PulseScale, Excited.PulseScale);
	return Out;
}

FLinearColor FVibH2OMoodLook::ColorAt(float Position) const
{
	FLinearColor Color = FMath::Lerp(ColorLow, ColorHigh, FMath::Clamp(Position, 0.0f, 1.0f));
	Color.A = 1.0f;
	return Color;
}

// ------------------------------------------------------------------ Palette

FVibH2OMoodPalette::FVibH2OMoodPalette()
{
	// Calm: deep water. Blues and indigo, slow, small, close to still. The
	// light withdraws into the bubble rather than going out.
	Calm.ColorLow = FLinearColor(0.008f, 0.020f, 0.16f, 1.0f);
	Calm.ColorHigh = FLinearColor(0.020f, 0.11f, 0.42f, 1.0f);
	Calm.Glow = 0.55f;
	Calm.StripeSpeed = 0.10f;
	Calm.Turbulence = 0.55f;
	Calm.FloatScale = 0.75f;
	Calm.FloatSpeedScale = 0.65f;
	Calm.DriftScale = 0.4f;
	Calm.SizeScale = 0.92f;
	Calm.PulseScale = 0.7f;

	// Normal: the aqua of the main scene's water - teal to pale green.
	Normal.ColorLow = FLinearColor(0.015f, 0.26f, 0.36f, 1.0f);
	Normal.ColorHigh = FLinearColor(0.10f, 0.62f, 0.52f, 1.0f);
	Normal.Glow = 1.0f;
	Normal.StripeSpeed = 0.30f;
	Normal.Turbulence = 1.0f;

	// Excited: warm. Amber to coral, fast striations, a restless surface,
	// bigger bubbles that beat harder. Linear values: what reads as amber on
	// screen is far greener in linear space than the sRGB picker suggests.
	Excited.ColorLow = FLinearColor(0.95f, 0.32f, 0.035f, 1.0f);
	Excited.ColorHigh = FLinearColor(1.0f, 0.06f, 0.045f, 1.0f);
	Excited.Glow = 1.9f;
	Excited.StripeSpeed = 0.90f;
	Excited.Turbulence = 1.9f;
	Excited.FloatScale = 1.35f;
	Excited.FloatSpeedScale = 1.6f;
	Excited.DriftScale = 1.5f;
	Excited.SizeScale = 1.12f;
	Excited.PulseScale = 1.6f;
}

const FVibH2OMoodLook& FVibH2OMoodPalette::Get(EVibH2OMood Mood) const
{
	switch (Mood)
	{
	case EVibH2OMood::Calm:
		return Calm;
	case EVibH2OMood::Excited:
		return Excited;
	case EVibH2OMood::Normal:
	default:
		return Normal;
	}
}

FVibH2OMoodLook FVibH2OMoodPalette::Blend(const FVibH2OMoodWeights& Weights) const
{
	return FVibH2OMoodLook::Blend(Calm, Normal, Excited, Weights);
}

// ------------------------------------------------------------------ Tracker

EVibH2OMood FVibH2OMoodTracker::Classify(float Excitation, EVibH2OMood Current, const FVibH2OMoodTrackerSettings& Settings)
{
	// Thresholds typed in the wrong order still mean something.
	const float Low = FMath::Min(Settings.CalmThreshold, Settings.ExcitedThreshold);
	const float High = FMath::Max(Settings.CalmThreshold, Settings.ExcitedThreshold);
	const float Margin = FMath::Max(Settings.Hysteresis, 0.0f);

	switch (Current)
	{
	case EVibH2OMood::Calm:
		if (Excitation > High + Margin)
		{
			return EVibH2OMood::Excited;
		}
		return (Excitation > Low + Margin) ? EVibH2OMood::Normal : EVibH2OMood::Calm;

	case EVibH2OMood::Excited:
		if (Excitation < Low - Margin)
		{
			return EVibH2OMood::Calm;
		}
		return (Excitation < High - Margin) ? EVibH2OMood::Normal : EVibH2OMood::Excited;

	case EVibH2OMood::Normal:
	default:
		if (Excitation < Low - Margin)
		{
			return EVibH2OMood::Calm;
		}
		return (Excitation > High + Margin) ? EVibH2OMood::Excited : EVibH2OMood::Normal;
	}
}

static EVibH2OMood MoodFromOverride(EVibH2OMoodOverride Override, EVibH2OMood Fallback)
{
	switch (Override)
	{
	case EVibH2OMoodOverride::Calm:
		return EVibH2OMood::Calm;
	case EVibH2OMoodOverride::Normal:
		return EVibH2OMood::Normal;
	case EVibH2OMoodOverride::Excited:
		return EVibH2OMood::Excited;
	case EVibH2OMoodOverride::Auto:
	default:
		return Fallback;
	}
}

void FVibH2OMoodTracker::StartTransition(EVibH2OMood NewMood, float TransitionSeconds)
{
	Mood = NewMood;
	// From wherever the blend stands now, not from the previous mood's pure
	// look: a change of mind mid-fade turns round smoothly instead of
	// snapping back to a clean state first.
	From = Weights;
	Progress = (TransitionSeconds > KINDA_SMALL_NUMBER) ? 0.0f : 1.0f;
	if (Progress >= 1.0f)
	{
		Weights = FVibH2OMoodWeights::For(Mood);
	}
}

bool FVibH2OMoodTracker::Update(float CollectiveExcitation, bool bHasData, float DeltaSeconds, const FVibH2OMoodTrackerSettings& Settings)
{
	const float Dt = FMath::Max(DeltaSeconds, 0.0f);

	if (bHasData)
	{
		if (!bInitialised)
		{
			// First reading: no history to smooth against. Classifying from
			// "normal" gives the plain thresholds, without hysteresis.
			Smoothed = CollectiveExcitation;
			bInitialised = true;
		}
		else
		{
			const float Tau = Settings.DetectionSmoothingSeconds;
			const float Alpha = (Tau > KINDA_SMALL_NUMBER) ? (1.0f - FMath::Exp(-Dt / Tau)) : 1.0f;
			Smoothed += (CollectiveExcitation - Smoothed) * Alpha;
		}
	}

	const EVibH2OMood Detected = bInitialised ? Classify(Smoothed, Mood, Settings) : Mood;
	const EVibH2OMood Target = MoodFromOverride(Settings.Override, Detected);

	bool bChanged = false;
	if (Target != Mood)
	{
		StartTransition(Target, Settings.TransitionSeconds);
		bChanged = true;
	}

	if (Progress < 1.0f)
	{
		Progress = FMath::Min(1.0f, Progress + Dt / FMath::Max(Settings.TransitionSeconds, KINDA_SMALL_NUMBER));
		// Eased at both ends, so a fade neither starts nor lands with a kink.
		Weights = FVibH2OMoodWeights::Lerp(From, FVibH2OMoodWeights::For(Mood), FMath::SmoothStep(0.0f, 1.0f, Progress));
	}

	return bChanged;
}

void FVibH2OMoodTracker::Snap(float CollectiveExcitation, const FVibH2OMoodTrackerSettings& Settings)
{
	Smoothed = CollectiveExcitation;
	bInitialised = true;
	Mood = MoodFromOverride(Settings.Override, Classify(Smoothed, EVibH2OMood::Normal, Settings));
	Weights = FVibH2OMoodWeights::For(Mood);
	From = Weights;
	Progress = 1.0f;
}
