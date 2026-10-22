// VibH2O.EffectIsolation - does each effect move ONLY what it claims to move?
//
// Recipe criterion 8. The automation tests already prove that a toggle
// toggles: after /Effect/Drift/ 0, IsEffectEnabled returns false. What they do
// not prove is the part that shows on stage - that switching one effect off
// leaves the other five alone. A drift that quietly tinted, or a focus that
// nudged positions, would pass every existing test.
//
// The command builds a room by injection, drives it into a state where every
// effect has something to do, then switches the effects off ONE AT A TIME and
// measures seven observable channels each time:
//
//   position, rotation, echelle, silhouette, teinte, focus, battement
//
// The measurement problem, and the reason this file is longer than it looks:
// float, beat and organic deform are periodic, and the drift's direction turns
// with the stage clock, so two windows on the SAME state never return the same
// numbers. A hand-picked tolerance would therefore be arbitrary. So the
// reference is measured BETWEEN EVERY EFFECT, and each effect is compared to
// the midpoint of the two references framing it - which cancels any slow
// wander. The instrument then calibrates itself: every interior reference is
// judged against its own two neighbours by that same formula, and the worst
// figure it returns is the noise floor. A channel counts as moved only when it
// clears that floor threefold.
//
// Measured with a single reference at each end first: over thirty seconds the
// drift alone moved the bubbles further than any effect did, and every verdict
// came back SANS EFFET. The interleaving is not caution, it is the fix.
//
// Run it headless, alongside the visual sweep:
//   UnrealEditor.exe <project>.uproject -game -Windowed -ResX=1280 -ResY=720
//       -ExecCmds="VibH2O.EffectIsolation" -NoSound -unattended -nosplash
//
// Screenshots land in Saved/VibH2OIsolation/, one per effect, all from the
// same fixed camera so they can be flicked through and compared. With -nullrhi
// there is no viewport and no screenshot, but the numeric verdict still holds.
//
// Not compiled into Shipping: it is a verification harness, not show code.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Osc/VibH2OOscTypes.h"
#include "UnrealClient.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"
#include "VibH2OSimulatorActor.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"

namespace VibH2OEffectIsolationInternal
{

// --------------------------------------------------------------- Channels

/**
 * The observable channels - what a spectator could point at and say "that
 * changed".
 *
 * Echelle and Silhouette are deliberately kept apart. Echelle is the uniform
 * component of the scale, Silhouette its anisotropy: a bubble can grow without
 * changing shape, and wobble without changing size. Merging them would hide
 * exactly the kind of coupling this command exists to find.
 */
enum class EChannel : uint8
{
	Position,
	Rotation,
	Echelle,
	Silhouette,
	Teinte,
	Focus,
	Battement,
	Count
};

static constexpr int32 ChannelCount = static_cast<int32>(EChannel::Count);

static const TCHAR* ChannelName(EChannel Channel)
{
	switch (Channel)
	{
	case EChannel::Position:   return TEXT("position");
	case EChannel::Rotation:   return TEXT("rotation");
	case EChannel::Echelle:    return TEXT("echelle");
	case EChannel::Silhouette: return TEXT("silhouette");
	case EChannel::Teinte:     return TEXT("teinte");
	case EChannel::Focus:      return TEXT("focus");
	case EChannel::Battement:  return TEXT("battement");
	default:                   return TEXT("?");
	}
}

/**
 * What each effect is ALLOWED to move.
 *
 * This table is the contract being verified, and it is a statement of intent,
 * not a description of the code - which is the whole point: if the code and
 * this table disagree, one of the two is wrong and the run says so.
 *
 * The two couplings below are deliberate, and both are worth knowing about:
 *
 *  - Beat also moves Echelle: the beat pop is a size pulse, so silencing the
 *    beat necessarily calms the size.
 *  - Scale also moves Silhouette: the organic deformation is gated by the same
 *    toggle, so switching size response off also returns every bubble to a
 *    perfect sphere.
 */
static bool IsDeclared(FName Effect, EChannel Channel)
{
	if (Effect == VibH2OEffects::Float)
	{
		return Channel == EChannel::Position || Channel == EChannel::Rotation;
	}
	if (Effect == VibH2OEffects::Drift)
	{
		return Channel == EChannel::Position;
	}
	if (Effect == VibH2OEffects::Scale)
	{
		return Channel == EChannel::Echelle || Channel == EChannel::Silhouette;
	}
	if (Effect == VibH2OEffects::Tint)
	{
		return Channel == EChannel::Teinte;
	}
	if (Effect == VibH2OEffects::Focus)
	{
		return Channel == EChannel::Focus;
	}
	if (Effect == VibH2OEffects::Beat)
	{
		return Channel == EChannel::Battement || Channel == EChannel::Echelle;
	}
	return false;
}

// -------------------------------------------------------------- Statistics

/**
 * Mean and peak-to-peak of one channel, for one bubble, over one window.
 *
 * Both are needed. Switching the drift off shifts a bubble's mean position
 * without changing how much it wanders; switching the float off collapses how
 * much it wanders without moving its mean. Either alone would miss half the
 * effects.
 */
struct FAccumulator
{
	FVector Sum = FVector::ZeroVector;
	FVector Min = FVector(TNumericLimits<float>::Max());
	FVector Max = FVector(-TNumericLimits<float>::Max());
	int32 Count = 0;

	void Add(const FVector& Value)
	{
		Sum += Value;
		Min = Min.ComponentMin(Value);
		Max = Max.ComponentMax(Value);
		++Count;
	}

	FVector Mean() const
	{
		return (Count > 0) ? Sum / static_cast<double>(Count) : FVector::ZeroVector;
	}

	FVector PeakToPeak() const
	{
		return (Count > 0) ? (Max - Min) : FVector::ZeroVector;
	}
};

/** Every channel of one bubble. */
struct FBubbleSample
{
	FAccumulator Channels[ChannelCount];
};

/** One measurement window: every bubble, keyed by seat index. */
struct FRun
{
	FString Label;
	FName EffectOff = NAME_None;
	TMap<int32, FBubbleSample> BySeat;
};

/**
 * Distance between a run and the MIDPOINT of the two reference runs framing
 * it, averaged over the bubbles they share.
 *
 * Not a plain A/B against a single reference, and the reason is the drift: its
 * direction turns slowly with the stage clock, so the room genuinely is not in
 * the same place thirty seconds later. Comparing against one reference would
 * charge that slow wander to whichever effect happened to be switched off at
 * the time. Framing each measurement between two references and comparing
 * against their midpoint cancels any drift that is linear over the step - and
 * over four seconds, it is.
 *
 * Units differ from one channel to the next - centimetres, degrees, a mask in
 * [0,1] - which is harmless: a channel is only ever compared against its own
 * noise floor, never against another channel.
 */
static double MidpointDistance(const FRun& Before, const FRun& Target, const FRun& After, EChannel Channel)
{
	const int32 Index = static_cast<int32>(Channel);
	double Total = 0.0;
	int32 Shared = 0;

	for (const TPair<int32, FBubbleSample>& Entry : Target.BySeat)
	{
		const FBubbleSample* Left = Before.BySeat.Find(Entry.Key);
		const FBubbleSample* Right = After.BySeat.Find(Entry.Key);
		if (Left == nullptr || Right == nullptr)
		{
			continue;
		}
		const FAccumulator& Sample = Entry.Value.Channels[Index];
		const FVector ReferenceMean = (Left->Channels[Index].Mean() + Right->Channels[Index].Mean()) * 0.5;
		const FVector ReferencePtp = (Left->Channels[Index].PeakToPeak() + Right->Channels[Index].PeakToPeak()) * 0.5;

		Total += (Sample.Mean() - ReferenceMean).Size() + (Sample.PeakToPeak() - ReferencePtp).Size();
		++Shared;
	}

	return (Shared > 0) ? Total / Shared : 0.0;
}

/** Typical size of a channel in a run - the yardstick for "2% of it". */
static double ChannelMagnitude(const FRun& Run, EChannel Channel)
{
	const int32 Index = static_cast<int32>(Channel);
	double Total = 0.0;

	for (const TPair<int32, FBubbleSample>& Entry : Run.BySeat)
	{
		const FAccumulator& Acc = Entry.Value.Channels[Index];
		Total += Acc.Mean().Size() + Acc.PeakToPeak().Size();
	}

	return (Run.BySeat.Num() > 0) ? Total / Run.BySeat.Num() : 0.0;
}

// ------------------------------------------------------------------- Run

class FEffectIsolation : public TSharedFromThis<FEffectIsolation>
{
public:
	static void Start(const TArray<FString>& Args, UWorld* InWorld);

	~FEffectIsolation()
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		}
	}

private:
	/** Long enough for the value smoothing and the beat pulse decay to settle. */
	static constexpr float SettleSeconds = 1.0f;
	/** Several float periods, so a periodic channel is measured over whole cycles. */
	static constexpr float MeasureSeconds = 2.5f;
	/** The screenshot needs a presented frame before the step moves on. */
	static constexpr float ShotSeconds = 0.6f;

	bool Tick(float DeltaSeconds);
	void EnterStep(int32 Index);
	void Sample();
	void Report();
	void Finish();

	UVibH2OSubsystem* GetSubsystem() const;
	void BuildRoom();
	void FeedData();
	void PlaceCamera();
	void Screenshot(const FString& Name);

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AVibH2OStageActor> Stage;
	TWeakObjectPtr<ACameraActor> Camera;

	TArray<FRun> Runs;
	int32 StepIndex = INDEX_NONE;
	float StepTime = 0.0f;
	float FeedAccumulator = 1.0f;
	bool bShotRequested = false;
	bool bQuitWhenDone = true;

	FTSTicker::FDelegateHandle TickerHandle;
	static TSharedPtr<FEffectIsolation> Active;
};

TSharedPtr<FEffectIsolation> FEffectIsolation::Active;

void FEffectIsolation::Start(const TArray<FString>& Args, UWorld* InWorld)
{
	if (InWorld == nullptr)
	{
		return;
	}
	if (Active.IsValid())
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: EffectIsolation deja en cours."));
		return;
	}

	AVibH2OStageActor* Found = nullptr;
	for (TActorIterator<AVibH2OStageActor> It(InWorld); It; ++It)
	{
		Found = *It;
		break;
	}
	if (Found == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: EffectIsolation — aucun acteur de scene dans le niveau."));
		return;
	}

	TSharedPtr<FEffectIsolation> Isolation = MakeShared<FEffectIsolation>();
	Isolation->World = InWorld;
	Isolation->Stage = Found;
	// "rester" keeps the editor alive at the end, for a run launched by hand
	// from the console. The automated run wants the process to exit.
	for (const FString& Arg : Args)
	{
		if (Arg.Equals(TEXT("rester"), ESearchCase::IgnoreCase))
		{
			Isolation->bQuitWhenDone = false;
		}
	}

	// A reference before AND after every effect. Measuring the reference only
	// at the two ends was tried first and buried every verdict: over thirty
	// seconds the drift turns far enough that the room is honestly elsewhere,
	// and that wander drowned the effects it was supposed to reveal.
	auto AddEffect = [&Isolation](const TCHAR* Label, FName Effect)
	{
		Isolation->Runs.Add({ FString::Printf(TEXT("reference_%d"), Isolation->Runs.Num() / 2), NAME_None, {} });
		Isolation->Runs.Add({ Label, Effect, {} });
	};

	AddEffect(TEXT("sans_flottement"), VibH2OEffects::Float);
	AddEffect(TEXT("sans_derive"), VibH2OEffects::Drift);
	AddEffect(TEXT("sans_battement"), VibH2OEffects::Beat);
	AddEffect(TEXT("sans_teinte"), VibH2OEffects::Tint);
	AddEffect(TEXT("sans_focus"), VibH2OEffects::Focus);
	AddEffect(TEXT("sans_echelle"), VibH2OEffects::Scale);
	Isolation->Runs.Add({ FString::Printf(TEXT("reference_%d"), Isolation->Runs.Num() / 2), NAME_None, {} });

	Isolation->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(Isolation.ToSharedRef(), &FEffectIsolation::Tick));
	Active = Isolation;

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: EffectIsolation demarre — %d mesures."), Isolation->Runs.Num());
}

UVibH2OSubsystem* FEffectIsolation::GetSubsystem() const
{
	return World.IsValid() ? UVibH2OSubsystem::Get(World.Get()) : nullptr;
}

void FEffectIsolation::BuildRoom()
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr || !Stage.IsValid())
	{
		return;
	}

	// 5 x 4, not square: the same reason the room order is validated on 7 x 3.
	constexpr int32 Columns = 5;
	constexpr int32 Rows = 4;
	TArray<int32> Ids;
	Ids.Reserve(Columns * Rows);
	for (int32 Index = 1; Index <= Columns * Rows; ++Index)
	{
		Ids.Add(Index);
	}
	Subsystem->SetRoomManually(Columns, Rows, Ids);

	AVibH2OStageActor* S = Stage.Get();
	// A flat, straight room: curvature and rake would only add motionless
	// offsets to the position channel and dilute the numbers being compared.
	S->CurvatureAngle = 0.0f;
	S->ElevationPerRow = 0.0f;
	S->DepthAmplitude = 0.0f;
	S->BlendAlpha = 0.0f;
	S->bShowSeatNumbers = false;

	// The clocks are stopped, the contributions are not. Float, drift direction
	// and organic deform all read the stage clock, and none of their periods
	// divides a measurement window - so their own turning showed up as movement
	// and swamped the effects. At zero speed each one holds the offset its seed
	// gives it: still non-zero, still specific to that bubble, so switching the
	// effect off still removes something measurable. What this run judges is
	// therefore WHICH channels an effect touches, never how well it animates -
	// that question belongs to the sweep and to the eye.
	S->FloatSpeed = 0.0f;
	S->DriftDirectionSpeed = 0.0f;
	S->OrganicSpeed = 0.0f;

	// Focus has to have something to dim, or switching it off would prove
	// nothing at all - the trap this whole command is built to avoid.
	FVibH2OFocusGroup Group;
	Group.GroupName = TEXT("IsolationGauche");
	Group.bUseRect = true;
	Group.ColumnMin = 0;
	Group.ColumnMax = 1;
	Group.RowMin = 0;
	Group.RowMax = Rows - 1;
	S->FocusGroups.Add(Group);
	S->SetActiveFocusGroup(Group.GroupName);
	S->FocusAmount = 1.0f;

	// The built-in simulator plays Max from inside the level and starts on its
	// own on the demo map. Left running, it would keep injecting its own BPM and
	// arousal over the constant values below - which is precisely what the first
	// run of this command measured, and why its noise floor buried everything.
	int32 StoppedSimulators = 0;
	for (TActorIterator<AVibH2OSimulatorActor> It(World.Get()); It; ++It)
	{
		It->StopSimulation();
		++StoppedSimulators;
	}
	if (StoppedSimulators > 0)
	{
		UE_LOG(LogVibH2O, Display,
			TEXT("VibH2O: ISOLATION %d simulateur(s) arrete(s) — la mesure fournit elle-meme les donnees."),
			StoppedSimulators);
	}

	FeedData();
	FeedData();
}

void FEffectIsolation::FeedData()
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}

	for (const FVibH2OSeat& Seat : Subsystem->GetRoomRef().Seats)
	{
		if (!Seat.bOccupied)
		{
			continue;
		}
		const int32 Id = Seat.IndividualId;

		// CONSTANT per individual, and never a function of time. The sweep
		// feeds sinusoids to make its captures lively; here that would make the
		// two reference windows differ for a reason that has nothing to do with
		// the effects, and every verdict would be noise.
		const float Bpm = 60.0f + static_cast<float>((Id * 7) % 40);
		// Above DriftThreshold, or the drift would have nothing to do either.
		const float Excitation = 0.75f;
		const float Synchrony = 0.5f;

		FVibH2OOscMessage Message(FString::Printf(TEXT("/BPM/%d/"), Id));
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Bpm));
		Subsystem->InjectOscMessage(Message);

		Message = FVibH2OOscMessage(FString::Printf(TEXT("/SD/%d/"), Id));
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Excitation));
		Subsystem->InjectOscMessage(Message);

		Message = FVibH2OOscMessage(FString::Printf(TEXT("/Synchronie/%d/"), Id));
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Synchrony));
		Subsystem->InjectOscMessage(Message);
	}
}

void FEffectIsolation::PlaceCamera()
{
	if (!World.IsValid() || !Stage.IsValid() || Camera.IsValid())
	{
		return;
	}

	FBox Bounds(ForceInit);
	for (AVibH2OBubbleActor* Bubble : Stage->GetBubbles())
	{
		Bounds += Bubble->GetActorLocation();
	}
	if (!Bounds.IsValid)
	{
		Bounds = FBox(Stage->GetActorLocation(), Stage->GetActorLocation());
	}
	Bounds = Bounds.ExpandBy(80.0f);

	const FVector Center = Bounds.GetCenter();
	const float Radius = FMath::Max(static_cast<float>(Bounds.GetExtent().Size()), 200.0f);
	const FVector Location = Center + FVector(-0.75f, -0.35f, 0.5f).GetSafeNormal() * Radius * 1.5f;

	// Placed once and never moved again: two captures are only comparable by
	// eye if the camera did not move between them.
	Camera = World->SpawnActor<ACameraActor>(Location, (Center - Location).Rotation());
	if (Camera.IsValid())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(Camera.Get(), 0.0f);
		}
	}
}

void FEffectIsolation::Screenshot(const FString& Name)
{
	// No viewport under -nullrhi. The numbers are still worth having, so the
	// run carries on rather than refusing to start.
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return;
	}
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("VibH2OIsolation");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString File = Dir / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: capture %s"), *File);
}

void FEffectIsolation::EnterStep(int32 Index)
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		Finish();
		return;
	}

	// Whatever the previous step switched off comes back on: only ever one
	// effect is missing at a time, which is what "isolated" means here.
	if (Runs.IsValidIndex(StepIndex) && !Runs[StepIndex].EffectOff.IsNone())
	{
		Subsystem->SetEffectEnabled(Runs[StepIndex].EffectOff, true);
	}

	StepIndex = Index;
	StepTime = 0.0f;
	bShotRequested = false;

	if (!Runs.IsValidIndex(StepIndex))
	{
		Report();
		Finish();
		return;
	}

	if (!Runs[StepIndex].EffectOff.IsNone())
	{
		Subsystem->SetEffectEnabled(Runs[StepIndex].EffectOff, false);
	}

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: ISOLATION mesure %d/%d — %s"),
		StepIndex + 1, Runs.Num(), *Runs[StepIndex].Label);
}

void FEffectIsolation::Sample()
{
	if (!Stage.IsValid() || !Runs.IsValidIndex(StepIndex))
	{
		return;
	}
	FRun& Run = Runs[StepIndex];

	for (AVibH2OBubbleActor* Bubble : Stage->GetBubbles())
	{
		if (!IsValid(Bubble))
		{
			continue;
		}
		FBubbleSample& Sample = Run.BySeat.FindOrAdd(Bubble->SeatIndex);

		const FVector Location = Bubble->GetRootComponent()->GetRelativeLocation();
		const FVector Euler = Bubble->GetRootComponent()->GetRelativeRotation().Euler();
		const FVector Scale = Bubble->GetRootComponent()->GetRelativeScale3D();

		Sample.Channels[static_cast<int32>(EChannel::Position)].Add(Location);
		Sample.Channels[static_cast<int32>(EChannel::Rotation)].Add(Euler);

		// Size and shape, kept apart: the uniform part on one side, the three
		// differences between axes on the other - DIVIDED by the uniform part, so
		// the shape figure says nothing about the size.
		//
		// Measured without that division first, and it reported the beat leaking
		// into the silhouette: the deformation is a factor applied to the scale,
		// so the beat pop was inflating the absolute gap between axes without
		// changing the shape one bit. The leak was in the instrument, not in the
		// plugin.
		const double Uniform = (Scale.X + Scale.Y + Scale.Z) / 3.0;
		const double ShapeDivisor = FMath::Max(Uniform, UE_DOUBLE_KINDA_SMALL_NUMBER);
		Sample.Channels[static_cast<int32>(EChannel::Echelle)].Add(FVector(Uniform, 0.0, 0.0));
		Sample.Channels[static_cast<int32>(EChannel::Silhouette)].Add(
			FVector(Scale.X - Scale.Y, Scale.Y - Scale.Z, Scale.Z - Scale.X) / ShapeDivisor);

		Sample.Channels[static_cast<int32>(EChannel::Focus)].Add(FVector(Bubble->FocusMask, 0.0, 0.0));
		Sample.Channels[static_cast<int32>(EChannel::Battement)].Add(
			FVector(Bubble->State.BeatPhase, Bubble->State.BeatPulse, 0.0));

		// The tint is read back from the material instance rather than from the
		// bubble: what reaches the material is what the spectator sees, and it
		// is the only place the value exists after the push.
		if (UMaterialInstanceDynamic* Material = Bubble->GetDynamicMaterial())
		{
			if (!Bubble->ParameterNames.TintColor.IsNone())
			{
				const FLinearColor Tint = Material->K2_GetVectorParameterValue(Bubble->ParameterNames.TintColor);
				Sample.Channels[static_cast<int32>(EChannel::Teinte)].Add(FVector(Tint.R, Tint.G, Tint.B));
			}
		}
	}
}

void FEffectIsolation::Report()
{
	// References sit on the even indices, the effects between them on the odd
	// ones: 0 ref, 1 effet, 2 ref, 3 effet...
	if (Runs.Num() < 5)
	{
		return;
	}

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: ================ ISOLATION DES EFFETS ================"));
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: %d bulles mesurees, fenetre de %.1f s, %d references intercalees."),
		Runs[0].BySeat.Num(), MeasureSeconds, (Runs.Num() + 1) / 2);

	// The instrument measures itself first: each interior reference is judged
	// against its two neighbouring references, by exactly the formula used on
	// the effects. Whatever that returns is what "no change at all" costs.
	double Noise[ChannelCount] = { 0.0 };
	double Threshold[ChannelCount];
	for (int32 Index = 0; Index < ChannelCount; ++Index)
	{
		const EChannel Channel = static_cast<EChannel>(Index);
		for (int32 RunIndex = 2; RunIndex + 2 < Runs.Num(); RunIndex += 2)
		{
			// The worst of them, not the average: a noise floor that flatters
			// the instrument turns measurement slop into a finding.
			Noise[Index] = FMath::Max(Noise[Index],
				MidpointDistance(Runs[RunIndex - 2], Runs[RunIndex], Runs[RunIndex + 2], Channel));
		}
		// Three times the noise floor, and never less than 2% of the channel's
		// own size - the second term keeps a channel that happens to be
		// perfectly still from calling every rounding error a change.
		Threshold[Index] = FMath::Max(3.0 * Noise[Index], 0.02 * ChannelMagnitude(Runs[0], Channel));
	}

	FString NoiseLine;
	for (int32 Index = 0; Index < ChannelCount; ++Index)
	{
		NoiseLine += FString::Printf(TEXT("%s: bruit %.4f seuil %.4f | "),
			ChannelName(static_cast<EChannel>(Index)), Noise[Index], Threshold[Index]);
	}
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: instrument — %s"), *NoiseLine);

	int32 Conformes = 0;
	int32 Ecarts = 0;

	for (int32 RunIndex = 1; RunIndex < Runs.Num(); RunIndex += 2)
	{
		const FRun& Run = Runs[RunIndex];
		FString Line;
		FString Surprises;

		for (int32 Index = 0; Index < ChannelCount; ++Index)
		{
			const EChannel Channel = static_cast<EChannel>(Index);
			const double Distance = MidpointDistance(Runs[RunIndex - 1], Run, Runs[RunIndex + 1], Channel);
			const bool bMoved = Distance > Threshold[Index];
			const bool bExpected = IsDeclared(Run.EffectOff, Channel);

			Line += FString::Printf(TEXT("%s%s=%.4f%s "),
				bMoved ? TEXT("*") : TEXT(" "), ChannelName(Channel), Distance,
				bExpected ? TEXT("(attendu)") : TEXT(""));

			if (bMoved && !bExpected)
			{
				Surprises += FString::Printf(TEXT("FUITE sur %s; "), ChannelName(Channel));
			}
			else if (!bMoved && bExpected)
			{
				Surprises += FString::Printf(TEXT("SANS EFFET sur %s; "), ChannelName(Channel));
			}
		}

		if (Surprises.IsEmpty())
		{
			++Conformes;
		}
		else
		{
			++Ecarts;
		}

		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: %-16s %s"), *Run.Label, *Line);
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: %-16s VERDICT=%s %s"),
			*Run.Label, Surprises.IsEmpty() ? TEXT("CONFORME") : TEXT("ECART"), *Surprises);
	}

	UE_LOG(LogVibH2O, Display,
		TEXT("VibH2O: ISOLATION TERMINEE — %d conformes, %d ecarts. Captures dans Saved/VibH2OIsolation/."),
		Conformes, Ecarts);
}

bool FEffectIsolation::Tick(float DeltaSeconds)
{
	if (!World.IsValid() || !Stage.IsValid())
	{
		Finish();
		return false;
	}

	if (StepIndex == INDEX_NONE)
	{
		BuildRoom();
		PlaceCamera();
		EnterStep(0);
		return true;
	}

	// Kept fed throughout, with the same constant values: a sensor that fell
	// silent mid-run would go stale, stop beating, and be blamed on whichever
	// effect happened to be switched off at that moment.
	FeedAccumulator += DeltaSeconds;
	if (FeedAccumulator >= 0.5f)
	{
		FeedAccumulator = 0.0f;
		FeedData();
	}

	StepTime += DeltaSeconds;

	if (StepTime >= SettleSeconds && StepTime < SettleSeconds + MeasureSeconds)
	{
		Sample();
	}
	else if (StepTime >= SettleSeconds + MeasureSeconds)
	{
		if (!bShotRequested)
		{
			bShotRequested = true;
			Screenshot(FString::Printf(TEXT("%02d_%s"), StepIndex, *Runs[StepIndex].Label));
		}
		else if (StepTime >= SettleSeconds + MeasureSeconds + ShotSeconds)
		{
			EnterStep(StepIndex + 1);
		}
	}

	return true;
}

void FEffectIsolation::Finish()
{
	if (bQuitWhenDone && World.IsValid() && GEngine != nullptr)
	{
		GEngine->Exec(World.Get(), TEXT("QUIT"));
	}
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Active.Reset();
}

static FAutoConsoleCommandWithWorldAndArgs GVibH2OEffectIsolationCommand(
	TEXT("VibH2O.EffectIsolation"),
	TEXT("Coupe chaque effet a tour de role et mesure ce qui bouge vraiment. Argument 'rester' pour ne pas quitter."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FEffectIsolation::Start));

} // namespace VibH2OEffectIsolationInternal

#endif // !UE_BUILD_SHIPPING
