// VibH2O.DemoSweep - the self-driving visual recipe.
//
// The roadmap's remaining criteria are the ones a numeric test cannot judge:
// "does the curvature LOOK right", "does the focus dim", "does the morph read
// as a morph". This command turns them into screenshots. It builds a room by
// injection (no network needed), walks through every visual criterion of the
// recipe - curvature both ways, constant spacing, rake and relief, focus,
// quiet sensors, a moved stage actor, the vortex morph, a Sequencer playback,
// the 176-seat room with an FPS measurement - and saves one labelled PNG per
// phase under Saved/VibH2OSweep/, then quits.
//
// Run it headless:
//   UnrealEditor.exe <project>.uproject -game -RenderOffscreen
//       -ExecCmds="VibH2O.DemoSweep" -NoSound -unattended
//
// Not compiled into Shipping: it is a verification harness, not show code.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/Paths.h"
#include "Osc/VibH2OOscTypes.h"
#include "UnrealClient.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OSimulatorActor.h"

#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace VibH2ODemoSweepInternal
{

class FVibH2ODemoSweep : public TSharedFromThis<FVibH2ODemoSweep>
{
public:
	static void Start(UWorld* InWorld);

	~FVibH2ODemoSweep()
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		}
	}

private:
	struct FPhase
	{
		const TCHAR* Name = nullptr;
		float Duration = 2.5f;
		/** How far to pull the camera back, as a multiplier on the fit distance. */
		float CameraDistanceMul = 1.0f;
		/**
		 * Vertical component of the camera direction. What a phase proves
		 * decides where the camera stands: a curvature bow reads from above
		 * (2.0), a rake or a cone reads from the side (0.2), a room reads
		 * three-quarter (0.55).
		 */
		float CameraHeight = 0.55f;
		TFunction<void(FVibH2ODemoSweep&)> Enter;
		/** Called every tick with the phase progress in [0,1]. */
		TFunction<void(FVibH2ODemoSweep&, float)> Update;
	};

	bool Tick(float DeltaTime);
	void BuildPhases();
	void EnterPhase(int32 Index);

	UVibH2OSubsystem* GetSubsystem() const;
	void BuildRoom(int32 Columns, int32 Rows);
	void FeedData();
	void FrameCamera(float DistanceMul, float Height);
	void Screenshot(const FString& Name);
	void Finish();

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AVibH2OStageActor> Stage;
	TWeakObjectPtr<ACameraActor> Camera;

	TArray<FPhase> Phases;
	int32 PhaseIndex = INDEX_NONE;
	float PhaseTime = 0.0f;
	float TotalTime = 0.0f;
	bool bStarted = false;
	bool bReframed = false;
	bool bShotRequested = false;
	int32 SettleFrames = 0;
	int32 FramesAfterShot = 0;

	bool bFeedHalfOnly = false;
	float FeedAccumulator = 1.0f;

	bool bMeasureFps = false;
	float FpsAccumTime = 0.0f;
	int32 FpsFrames = 0;

	float SequencerBlendAtStart = 0.0f;

	FTSTicker::FDelegateHandle TickerHandle;
	static TSharedPtr<FVibH2ODemoSweep> Active;
};

TSharedPtr<FVibH2ODemoSweep> FVibH2ODemoSweep::Active;

void FVibH2ODemoSweep::Start(UWorld* InWorld)
{
	if (Active.IsValid())
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: DemoSweep deja en cours."));
		return;
	}

	TSharedPtr<FVibH2ODemoSweep> Sweep = MakeShared<FVibH2ODemoSweep>();
	Sweep->World = InWorld;
	Sweep->BuildPhases();
	Sweep->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(Sweep.ToSharedRef(), &FVibH2ODemoSweep::Tick));
	Active = Sweep;

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: DemoSweep demarre — %d phases."), Sweep->Phases.Num());
}

UVibH2OSubsystem* FVibH2ODemoSweep::GetSubsystem() const
{
	return World.IsValid() ? UVibH2OSubsystem::Get(World.Get()) : nullptr;
}

void FVibH2ODemoSweep::BuildRoom(int32 Columns, int32 Rows)
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}
	TArray<int32> Ids;
	Ids.Reserve(Columns * Rows);
	for (int32 Index = 1; Index <= Columns * Rows; ++Index)
	{
		Ids.Add(Index);
	}
	Subsystem->SetRoomManually(Columns, Rows, Ids);
	// Two immediate feeds so every individual is known and non-stale before
	// the first frame of the phase.
	FeedData();
	FeedData();
}

void FVibH2ODemoSweep::FeedData()
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
		if (bFeedHalfOnly && (Id % 2 == 0))
		{
			// This half goes quiet: the staleness path must desaturate them.
			continue;
		}

		const float Bpm = 55.0f + static_cast<float>((Id * 7) % 50);
		const float Excitation = 0.5f + 0.5f * FMath::Sin(TotalTime * 0.8f + Id * 0.7f);
		const float Synchrony = 0.5f + 0.5f * FMath::Sin(TotalTime * 0.25f + Id * 0.1f);

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

void FVibH2ODemoSweep::FrameCamera(float DistanceMul, float Height)
{
	if (!World.IsValid() || !Stage.IsValid())
	{
		return;
	}

	// Bounds over the bubbles' real world positions - the same rule the focus
	// framing uses, and for the same reason: an assumed grid would misframe a
	// curved or moved room.
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
	// The camera's horizontal FOV is 90 degrees: half of it already swallows
	// the bounding radius at equal distance, so the fit factor stays modest.
	const float Distance = Radius * 1.35f * DistanceMul;

	const FVector Direction = FVector(-0.75f, -0.35f, Height).GetSafeNormal();
	const FVector Location = Center + Direction * Distance;

	if (!Camera.IsValid())
	{
		Camera = World->SpawnActor<ACameraActor>(Location, FRotator::ZeroRotator);
	}
	if (Camera.IsValid())
	{
		Camera->SetActorLocation(Location);
		Camera->SetActorRotation((Center - Location).Rotation());
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(Camera.Get(), 0.0f);
		}
	}
}

void FVibH2ODemoSweep::Screenshot(const FString& Name)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("VibH2OSweep");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString File = Dir / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: capture %s"), *File);
}

void FVibH2ODemoSweep::BuildPhases()
{
	// Phase 0 - warm-up. Builds the 7 x 3 room dead straight and gives the
	// shader compiler time to finish before anything is judged visually.
	Phases.Add({ TEXT("01_grille_droite"), 4.0f, 0.9f, 0.55f,
		[](FVibH2ODemoSweep& Self)
		{
			GetMutableDefault<UVibH2OSettings>()->StaleTimeoutSeconds = 2.0f;
			Self.BuildRoom(7, 3);
			if (Self.Stage.IsValid())
			{
				AVibH2OStageActor* S = Self.Stage.Get();
				S->CurvatureAngle = 0.0f;
				S->ElevationPerRow = 0.0f;
				S->DepthAmplitude = 0.0f;
				S->bFanOut = true;
				S->BlendAlpha = 0.0f;
				S->FocusAmount = 0.0f;
			}
		}, nullptr });

	Phases.Add({ TEXT("02_courbure_positive"), 2.5f, 0.9f, 2.2f,
		[](FVibH2ODemoSweep& Self) { if (Self.Stage.IsValid()) { Self.Stage->CurvatureAngle = 10.0f; } }, nullptr });

	Phases.Add({ TEXT("03_courbure_negative"), 2.5f, 0.9f, 2.2f,
		[](FVibH2ODemoSweep& Self) { if (Self.Stage.IsValid()) { Self.Stage->CurvatureAngle = -10.0f; } }, nullptr });

	Phases.Add({ TEXT("04_espacement_constant"), 2.5f, 0.9f, 2.2f,
		[](FVibH2ODemoSweep& Self)
		{
			if (Self.Stage.IsValid()) { Self.Stage->CurvatureAngle = 8.0f; Self.Stage->bFanOut = false; }
		}, nullptr });

	Phases.Add({ TEXT("05_gradins_relief"), 2.5f, 0.9f, 0.2f,
		[](FVibH2ODemoSweep& Self)
		{
			if (Self.Stage.IsValid())
			{
				Self.Stage->bFanOut = true;
				Self.Stage->CurvatureAngle = 5.0f;
				Self.Stage->ElevationPerRow = 45.0f;
				Self.Stage->DepthAmplitude = 30.0f;
			}
		}, nullptr });

	Phases.Add({ TEXT("06_focus_haut_gauche"), 2.5f, 0.9f, 0.55f,
		[](FVibH2ODemoSweep& Self)
		{
			if (Self.Stage.IsValid())
			{
				Self.Stage->SetActiveFocusGroup(TEXT("HautGauche"));
				Self.Stage->FocusAmount = 1.0f;
			}
		}, nullptr });

	// Half the sensors stop talking; the 2 s timeout set at warm-up must turn
	// them grey well inside this phase.
	Phases.Add({ TEXT("07_capteurs_muets"), 3.5f, 0.9f, 0.55f,
		[](FVibH2ODemoSweep& Self)
		{
			if (Self.Stage.IsValid())
			{
				Self.Stage->SetActiveFocusGroup(NAME_None);
				Self.Stage->FocusAmount = 0.0f;
			}
			Self.bFeedHalfOnly = true;
		}, nullptr });

	Phases.Add({ TEXT("08_acteur_deplace"), 2.5f, 0.9f, 0.55f,
		[](FVibH2ODemoSweep& Self)
		{
			Self.bFeedHalfOnly = false;
			if (Self.Stage.IsValid())
			{
				Self.Stage->AddActorWorldOffset(FVector(150.0f, 250.0f, 120.0f));
				Self.Stage->AddActorWorldRotation(FRotator(0.0f, 35.0f, 0.0f));
			}
		}, nullptr });

	Phases.Add({ TEXT("09_morph_vers_vortex"), 3.0f, 1.1f, 0.2f,
		nullptr,
		[](FVibH2ODemoSweep& Self, float Progress)
		{
			if (Self.Stage.IsValid()) { Self.Stage->BlendAlpha = Progress; }
		} });

	Phases.Add({ TEXT("10_vortex_plein"), 2.5f, 1.1f, 0.15f,
		[](FVibH2ODemoSweep& Self) { if (Self.Stage.IsValid()) { Self.Stage->BlendAlpha = 1.0f; } }, nullptr });

	Phases.Add({ TEXT("11_retour_salle"), 2.5f, 1.0f, 0.4f,
		nullptr,
		[](FVibH2ODemoSweep& Self, float Progress)
		{
			if (Self.Stage.IsValid()) { Self.Stage->BlendAlpha = 1.0f - Progress; }
		} });

	// Sequencer takes the controls: the sweep stops writing BlendAlpha and the
	// Level Sequence generated by Tools/make_demo_sequence.py drives it.
	Phases.Add({ TEXT("12_sequencer_debut"), 1.5f, 1.0f, 0.55f,
		[](FVibH2ODemoSweep& Self)
		{
			if (!Self.World.IsValid())
			{
				return;
			}
			Self.SequencerBlendAtStart = Self.Stage.IsValid() ? Self.Stage->BlendAlpha : -1.0f;
			bool bFound = false;
			for (TActorIterator<ALevelSequenceActor> It(Self.World.Get()); It; ++It)
			{
				if (ULevelSequencePlayer* Player = It->GetSequencePlayer())
				{
					Player->Play();
					bFound = true;
					break;
				}
			}
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: sequenceur %s."),
				bFound ? TEXT("lance") : TEXT("INTROUVABLE — la verification Sequencer sera incomplete"));
		}, nullptr });

	Phases.Add({ TEXT("13_sequencer_en_lecture"), 2.0f, 1.0f, 0.55f,
		[](FVibH2ODemoSweep& Self)
		{
			// The sweep has not written BlendAlpha since the sequence started:
			// any change is Sequencer's doing, through the Interp properties.
			const float Now = Self.Stage.IsValid() ? Self.Stage->BlendAlpha : -1.0f;
			UE_LOG(LogVibH2O, Display,
				TEXT("VibH2O: Sequencer pilote BlendAlpha — %.3f au lancement, %.3f maintenant (%s)."),
				Self.SequencerBlendAtStart, Now,
				!FMath::IsNearlyEqual(Self.SequencerBlendAtStart, Now, 0.01f) ? TEXT("OK, la piste ecrit") : TEXT("AUCUN CHANGEMENT"));
		}, nullptr });

	Phases.Add({ TEXT("14_salle_176"), 5.0f, 0.9f, 0.7f,
		[](FVibH2ODemoSweep& Self)
		{
			if (Self.World.IsValid())
			{
				for (TActorIterator<ALevelSequenceActor> It(Self.World.Get()); It; ++It)
				{
					if (ULevelSequencePlayer* Player = It->GetSequencePlayer())
					{
						Player->Stop();
					}
				}
			}
			if (Self.Stage.IsValid()) { Self.Stage->BlendAlpha = 0.0f; }
			Self.BuildRoom(22, 8);
			Self.bMeasureFps = true;
			Self.FpsAccumTime = 0.0f;
			Self.FpsFrames = 0;
		}, nullptr });
}

void FVibH2ODemoSweep::EnterPhase(int32 Index)
{
	PhaseIndex = Index;
	PhaseTime = 0.0f;
	bReframed = false;
	bShotRequested = false;

	if (!Phases.IsValidIndex(Index))
	{
		Finish();
		return;
	}

	const FPhase& Phase = Phases[Index];
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: phase %s (%.1f s)"), Phase.Name, Phase.Duration);
	if (Phase.Enter)
	{
		Phase.Enter(*this);
	}
	FrameCamera(Phase.CameraDistanceMul, Phase.CameraHeight);
}

bool FVibH2ODemoSweep::Tick(float DeltaTime)
{
	if (!World.IsValid())
	{
		Active.Reset();
		return false;
	}

	// Wait until the world is actually playable: subsystem up, stage actor
	// found, a player controller to hang the view on.
	if (!bStarted)
	{
		UVibH2OSubsystem* Subsystem = GetSubsystem();
		if (Subsystem == nullptr || World->GetFirstPlayerController() == nullptr)
		{
			return true;
		}
		if (!Stage.IsValid())
		{
			for (TActorIterator<AVibH2OStageActor> It(World.Get()); It; ++It)
			{
				Stage = *It;
				break;
			}
			if (!Stage.IsValid())
			{
				UE_LOG(LogVibH2O, Error, TEXT("VibH2O: DemoSweep — aucun AVibH2OStageActor dans le niveau."));
				Finish();
				return false;
			}
		}
		// The demo map ships with an auto-starting simulator; the sweep drives
		// its own rooms and data, so the two would fight over the same
		// individuals. The sweep wins, explicitly.
		for (TActorIterator<AVibH2OSimulatorActor> It(World.Get()); It; ++It)
		{
			It->StopSimulation();
		}
#if WITH_EDITOR
		// Judging a screenshot rendered with placeholder shaders would be
		// judging the compiler queue, not the plugin.
		if (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling())
		{
			return true;
		}
#endif
		bStarted = true;
		// The camera teleports between phases; motion blur would smear every
		// screenshot taken after a jump.
		if (GEngine != nullptr)
		{
			GEngine->Exec(World.Get(), TEXT("r.MotionBlurQuality 0"));
		}
		EnterPhase(0);
		return true;
	}

	TotalTime += DeltaTime;
	PhaseTime += DeltaTime;

	// Live data at 10 Hz, through the same injection path the network uses.
	FeedAccumulator += DeltaTime;
	if (FeedAccumulator >= 0.1f)
	{
		FeedAccumulator = 0.0f;
		FeedData();
	}

	if (bMeasureFps && PhaseTime > 1.0f)
	{
		FpsAccumTime += DeltaTime;
		FpsFrames += 1;
	}

	const FPhase& Phase = Phases[PhaseIndex];
	if (Phase.Update)
	{
		Phase.Update(*this, FMath::Clamp(PhaseTime / Phase.Duration, 0.0f, 1.0f));
	}

	if (PhaseTime >= Phase.Duration)
	{
		if (!bReframed)
		{
			// Reframe on the settled positions: at phase entry the bubbles had
			// not yet moved to the new layout (in the very first phase they
			// were still at their spawn point). Then let a few frames pass so
			// temporal effects settle after the camera jump - a capture taken
			// on the jump frame comes out smeared.
			FrameCamera(Phase.CameraDistanceMul, Phase.CameraHeight);
			bReframed = true;
			SettleFrames = 5;
		}
		else if (!bShotRequested)
		{
			if (--SettleFrames > 0)
			{
				return true;
			}
			// The screenshot captures the end of THIS frame, while the phase
			// state is still in effect; the transition waits a few frames so
			// the capture is guaranteed to have landed.
			Screenshot(Phase.Name);
			bShotRequested = true;
			FramesAfterShot = 3;
		}
		else if (--FramesAfterShot <= 0)
		{
			if (bMeasureFps)
			{
				bMeasureFps = false;
				const float Fps = (FpsAccumTime > 0.0f) ? FpsFrames / FpsAccumTime : 0.0f;
				UE_LOG(LogVibH2O, Display,
					TEXT("VibH2O: salle 176 — %.1f images/s en moyenne sur %.1f s (%d images)."),
					Fps, FpsAccumTime, FpsFrames);
			}
			EnterPhase(PhaseIndex + 1);
		}
	}

	return true;
}

void FVibH2ODemoSweep::Finish()
{
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: DemoSweep termine — captures dans Saved/VibH2OSweep/."));
	if (World.IsValid() && GEngine != nullptr)
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

/**
 * VibH2O.Shot - capture what is ON SCREEN, without touching the camera.
 *
 * DemoSweep frames its own camera, so it cannot answer "what does someone see
 * when they just press Play". This one waits a couple of seconds, shoots the
 * current view as-is, and reports what the scene contains.
 */
static void ShotCommand(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<UWorld> WeakWorld(World);

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakWorld](float) -> bool
		{
			UWorld* W = WeakWorld.Get();
			if (W == nullptr)
			{
				return false;
			}

			int32 Bubbles = 0;
			for (TActorIterator<AVibH2OBubbleActor> It(W); It; ++It)
			{
				++Bubbles;
			}

			FString ViewInfo = TEXT("aucun controleur");
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				FVector Loc = FVector::ZeroVector;
				FRotator Rot = FRotator::ZeroRotator;
				PC->GetPlayerViewPoint(Loc, Rot);
				const AActor* Target = PC->GetViewTarget();
				ViewInfo = FString::Printf(TEXT("cible=%s pos=(%.0f, %.0f, %.0f) rot=(%.0f, %.0f)"),
					Target ? *Target->GetName() : TEXT("aucune"), Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw);
			}

			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SHOT %d bulles | vue %s"), Bubbles, *ViewInfo);

			const FString File = FPaths::ProjectSavedDir() / TEXT("VibH2OShot") / TEXT("vue_par_defaut.png");
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
			FScreenshotRequest::RequestScreenshot(File, false, false);

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[WeakWorld](float) -> bool
				{
					if (GEngine != nullptr && WeakWorld.IsValid())
					{
						GEngine->Exec(WeakWorld.Get(), TEXT("QUIT"));
					}
					return false;
				}), 1.0f);
			return false;
		}), 3.0f);
}

static FAutoConsoleCommandWithWorld GVibH2OShotCommand(
	TEXT("VibH2O.Shot"),
	TEXT("Capture la vue courante telle quelle, sans imposer de camera."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ShotCommand));

/**
 * VibH2O.PreviewProbe - does the stage actor tick in the EDITOR, no Play?
 *
 * StageTime is the clock every preview animation reads from, and it only
 * advances from Tick. Sampling it across a few seconds of editor ticking
 * answers the question with a number rather than an opinion. An FTSTicker runs
 * in the editor, which is exactly why the probe lives here and not in Python.
 */
static void PreviewProbe(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}

	AVibH2OStageActor* Stage = nullptr;
	for (TActorIterator<AVibH2OStageActor> It(World); It; ++It)
	{
		Stage = *It;
		break;
	}
	if (Stage == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: PREVIEW aucun acteur de scene."));
		return;
	}

	const float First = Stage->GetStageTime();
	const bool bGameWorld = World->IsGameWorld();
	// The world clock tells the two failure modes apart: a world that does not
	// advance means the viewport is not realtime, which no amount of actor
	// configuration would fix; a world that advances while the actor does not
	// means the tick registration is wrong.
	const double FirstWorld = World->GetTimeSeconds();
	const bool bActorTickEnabled = Stage->IsActorTickEnabled();
	const bool bViewportsOnly = Stage->ShouldTickIfViewportsOnly();
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<AVibH2OStageActor> WeakStage(Stage);

	// Sample again after three seconds of whatever ticking the editor does.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakStage, WeakWorld, First, FirstWorld, bGameWorld, bActorTickEnabled, bViewportsOnly](float) -> bool
		{
			const float Second = WeakStage.IsValid() ? WeakStage->GetStageTime() : -1.0f;
			const double SecondWorld = WeakWorld.IsValid() ? WeakWorld->GetTimeSeconds() : -1.0;
			UE_LOG(LogVibH2O, Display,
				TEXT("VibH2O: PREVIEW monde_de_jeu=%d tick_actif=%d viewports_only=%d | monde %.3f -> %.3f | StageTime %.3f -> %.3f — VERDICT=%s"),
				bGameWorld ? 1 : 0, bActorTickEnabled ? 1 : 0, bViewportsOnly ? 1 : 0,
				FirstWorld, SecondWorld, First, Second,
				(Second > First + 0.5f) ? TEXT("ANIME_SANS_PLAY") : TEXT("FIGE"));
			return false;
		}), 3.0f);
}

static FAutoConsoleCommandWithWorld GVibH2OPreviewProbeCommand(
	TEXT("VibH2O.PreviewProbe"),
	TEXT("Mesure si l'acteur de scene tourne dans l'editeur, hors Play."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&PreviewProbe));

static FAutoConsoleCommandWithWorld GVibH2ODemoSweepCommand(
	TEXT("VibH2O.DemoSweep"),
	TEXT("Deroule la recette visuelle complete et enregistre une capture par phase dans Saved/VibH2OSweep/, puis quitte."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&FVibH2ODemoSweep::Start));

} // namespace VibH2ODemoSweepInternal

#endif // !UE_BUILD_SHIPPING
