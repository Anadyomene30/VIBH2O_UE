// VibH2O.SchoolSweep - tableau 2 judged on the real data path.
//
// DemoSweep proves the room; this proves the school. It drives the level's
// simulator - the same actor that stands in for Max - on a 176-seat room,
// watches every bubble divide, sends the families to the school, then runs
// the "Balayage de synchronie" scenario and waits, by measurement rather than
// by stopwatch, for the school to disperse, gather into the vortex and
// disperse again. Each of those moments is logged with numbers - order,
// milling, size of the families - and captured as a PNG under
// Saved/VibH2OSchool/. Frame rate is measured with the full school on stage.
//
//   UnrealEditor.exe <project>.uproject -game -Windowed -ResX=1280 -ResY=720
//       -ExecCmds="VibH2O.SchoolSweep" -NoSound -unattended -nosplash
//
// Not compiled into Shipping: it is a verification harness, not show code.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OModule.h"
#include "VibH2OSimulatorActor.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"

#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace VibH2OSchoolSweepInternal
{

class FVibH2OSchoolSweep : public TSharedFromThis<FVibH2OSchoolSweep>
{
public:
	static void Start(UWorld* InWorld);

private:
	enum class EStep : uint8
	{
		Warmup,
		Divide,
		Leave,
		WaitDispersed,
		WaitVortex,
		WaitDispersedAgain,
		Return,
		Done
	};

	struct FSchoolReading
	{
		float Milling = 0.0f;
		float FamilyRadius = 0.0f;
		int32 Bodies = 0;
	};

	bool Tick(float DeltaTime);
	void Enter(EStep Next);
	FSchoolReading Read() const;
	void Frame(bool bCloseUp, float Height, float DistanceMul);
	void Shot(const TCHAR* Name);
	void Finish();

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AVibH2OStageActor> Stage;
	TWeakObjectPtr<AVibH2OSimulatorActor> Simulator;
	TWeakObjectPtr<ACameraActor> Camera;

	EStep Current = EStep::Warmup;
	bool bStarted = false;
	float StepTime = 0.0f;
	float SweepTime = 0.0f;
	float StableTime = 0.0f;
	float LogAccumulator = 0.0f;
	int32 PendingShotFrames = 0;
	FString PendingShot;

	float FpsTime = 0.0f;
	int32 FpsFrames = 0;
	float RoomFpsTime = 0.0f;
	int32 RoomFpsFrames = 0;

	FTSTicker::FDelegateHandle TickerHandle;
	static TSharedPtr<FVibH2OSchoolSweep> Active;
};

TSharedPtr<FVibH2OSchoolSweep> FVibH2OSchoolSweep::Active;

void FVibH2OSchoolSweep::Start(UWorld* InWorld)
{
	if (Active.IsValid())
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: SchoolSweep deja en cours."));
		return;
	}
	TSharedPtr<FVibH2OSchoolSweep> Sweep = MakeShared<FVibH2OSchoolSweep>();
	Sweep->World = InWorld;
	Sweep->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(Sweep.ToSharedRef(), &FVibH2OSchoolSweep::Tick));
	Active = Sweep;
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SchoolSweep demarre."));
}

FVibH2OSchoolSweep::FSchoolReading FVibH2OSchoolSweep::Read() const
{
	FSchoolReading Reading;
	if (!Stage.IsValid())
	{
		return Reading;
	}

	// Everything in the stage's local space: the axis of the vortex is local.
	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	TArray<int32> Families;
	TMap<int32, FVector> Centres;
	TMap<int32, int32> Sizes;

	TArray<AVibH2OBubbleActor*> Bodies = Stage->GetBubbles();
	Bodies.Append(Stage->GetDaughters());
	for (const AVibH2OBubbleActor* Body : Bodies)
	{
		if (Body->IsHidden())
		{
			continue;
		}
		const FVector P = Body->GetRootComponent()->GetRelativeLocation();
		// Facing is the velocity's direction: the boids driver turns each
		// bubble to where it swims.
		Positions.Add(P);
		Velocities.Add(Body->GetRootComponent()->GetRelativeRotation().Vector());
		Families.Add(Body->SeatIndex);
		Centres.FindOrAdd(Body->SeatIndex) += P;
		Sizes.FindOrAdd(Body->SeatIndex) += 1;
	}
	Reading.Bodies = Positions.Num();
	if (Reading.Bodies == 0)
	{
		return Reading;
	}

	FVector Mean = FVector::ZeroVector;
	for (const FVector& P : Positions)
	{
		Mean += P;
	}
	Mean /= Positions.Num();

	const float Spin = Stage->VortexSpinSpeed < 0.0f ? -1.0f : 1.0f;
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const FVector Rel(Positions[Index].X - Mean.X, Positions[Index].Y - Mean.Y, 0.0f);
		const FVector Heading(Velocities[Index].X, Velocities[Index].Y, 0.0f);
		if (Rel.SizeSquared() > 1.0 && Heading.SizeSquared() > 1e-4)
		{
			const FVector Tangent = FVector(-Rel.Y, Rel.X, 0.0f).GetSafeNormal() * Spin;
			Reading.Milling += static_cast<float>(FVector::DotProduct(Tangent, Heading.GetSafeNormal()));
		}
		const FVector Centre = Centres[Families[Index]] / Sizes[Families[Index]];
		Reading.FamilyRadius += static_cast<float>((Positions[Index] - Centre).Size());
	}
	Reading.Milling /= Positions.Num();
	Reading.FamilyRadius /= Positions.Num();
	return Reading;
}

void FVibH2OSchoolSweep::Frame(bool bCloseUp, float Height, float DistanceMul)
{
	if (!World.IsValid() || !Stage.IsValid())
	{
		return;
	}

	FBox Bounds(ForceInit);
	for (const AVibH2OBubbleActor* Bubble : Stage->GetBubbles())
	{
		// The close-up keeps a corner of the room - a few families, near
		// enough to see each one divide.
		if (!bCloseUp || (Bubble->Column < 4 && Bubble->Row < 3))
		{
			Bounds += Bubble->GetActorLocation();
		}
	}
	if (!bCloseUp)
	{
		for (const AVibH2OBubbleActor* Daughter : Stage->GetDaughters())
		{
			if (!Daughter->IsHidden())
			{
				Bounds += Daughter->GetActorLocation();
			}
		}
	}
	if (!Bounds.IsValid)
	{
		Bounds = FBox(Stage->GetActorLocation(), Stage->GetActorLocation());
	}
	Bounds = Bounds.ExpandBy(80.0f);

	const FVector Center = Bounds.GetCenter();
	const float Radius = FMath::Max(static_cast<float>(Bounds.GetExtent().Size()), 150.0f);
	const FVector Location = Center + FVector(-0.75f, -0.35f, Height).GetSafeNormal() * Radius * 1.35f * DistanceMul;

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

void FVibH2OSchoolSweep::Shot(const TCHAR* Name)
{
	// A few frames after framing, so temporal effects settle after the jump.
	PendingShot = Name;
	PendingShotFrames = 6;
}

void FVibH2OSchoolSweep::Enter(EStep Next)
{
	Current = Next;
	StepTime = 0.0f;
	StableTime = 0.0f;
	if (!Stage.IsValid())
	{
		return;
	}

	switch (Next)
	{
	case EStep::Divide:
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL division — %d meres, familles de %d."),
			Stage->GetBubbleCount(), Stage->GetFamilySize());
		break;
	case EStep::Leave:
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL depart vers le banc."));
		break;
	case EStep::WaitDispersed:
		// Restarting the simulation restarts its scenario clock: the sweep
		// begins at synchrony 0, climbs to 1 in 12.5 s and falls back by 25 s.
		if (Simulator.IsValid())
		{
			Simulator->StopSimulation();
			Simulator->StartSimulation();
		}
		SweepTime = 0.0f;
		FpsTime = 0.0f;
		FpsFrames = 0;
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL balayage de synchronie lance."));
		break;
	case EStep::Return:
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL retour au plan de salle."));
		break;
	default:
		break;
	}
}

bool FVibH2OSchoolSweep::Tick(float DeltaTime)
{
	if (!World.IsValid())
	{
		Active.Reset();
		return false;
	}

	if (!bStarted)
	{
		if (UVibH2OSubsystem::Get(World.Get()) == nullptr || World->GetFirstPlayerController() == nullptr)
		{
			return true;
		}
		for (TActorIterator<AVibH2OStageActor> It(World.Get()); It; ++It)
		{
			Stage = *It;
			break;
		}
		for (TActorIterator<AVibH2OSimulatorActor> It(World.Get()); It; ++It)
		{
			Simulator = *It;
			break;
		}
		if (!Stage.IsValid() || !Simulator.IsValid())
		{
			UE_LOG(LogVibH2O, Error, TEXT("VibH2O: SchoolSweep — il faut un acteur de scene ET un simulateur dans le niveau."));
			Finish();
			return false;
		}
#if WITH_EDITOR
		if (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling())
		{
			return true;
		}
#endif
		bStarted = true;
		if (GEngine != nullptr)
		{
			GEngine->Exec(World.Get(), TEXT("r.MotionBlurQuality 0"));
		}

		// The show's scale: 22 x 8 = 176 seats, the synchrony sweep.
		Simulator->Scenario = EVibH2OSimulatorScenario::SyncSweep;
		Simulator->AisleColumns.Reset();
		Simulator->ExtraEmptySeatIndices.Reset();
		Simulator->SetRoomSize(22, 8);
		Simulator->StartSimulation();
		Stage->BlendAlpha = 0.0f;
		Stage->bShowSeatNumbers = false;
		Enter(EStep::Warmup);
		return true;
	}

	StepTime += DeltaTime;

	if (PendingShotFrames > 0 && --PendingShotFrames == 0)
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("VibH2OSchool");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / (PendingShot + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: capture %s"), *File);
	}

	UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(World.Get());
	const float Sync = Subsystem ? Subsystem->GetCollectiveSynchrony() : 0.0f;
	const float Order = Stage.IsValid() ? Stage->GetSchoolOrder() : 0.0f;

	switch (Current)
	{
	case EStep::Warmup:
		if (StepTime > 1.0f)
		{
			RoomFpsTime += DeltaTime;
			RoomFpsFrames += 1;
		}
		if (StepTime >= 5.0f)
		{
			Frame(true, 0.6f, 1.0f);
			Shot(TEXT("01_salle_coin"));
			Enter(EStep::Divide);
		}
		break;

	case EStep::Divide:
		// Slowly enough to read: four seconds for the whole division.
		Stage->BlendAlpha = FMath::Clamp(StepTime / 4.0f, 0.0f, 1.0f) * Stage->DivisionEnd;
		if (FMath::IsNearlyEqual(Stage->GetDivisionProgress(), 0.4f, 0.02f) && PendingShot != TEXT("02_division_bourgeons"))
		{
			Frame(true, 0.6f, 1.0f);
			Shot(TEXT("02_division_bourgeons"));
		}
		if (StepTime >= 4.5f)
		{
			Frame(true, 0.6f, 1.0f);
			Shot(TEXT("03_division_achevee"));
			Enter(EStep::Leave);
		}
		break;

	case EStep::Leave:
		Stage->BlendAlpha = FMath::Lerp(Stage->DivisionEnd, 1.0f, FMath::Clamp(StepTime / 5.0f, 0.0f, 1.0f));
		if (StepTime >= 5.5f)
		{
			Enter(EStep::WaitDispersed);
		}
		break;

	case EStep::WaitDispersed:
	case EStep::WaitVortex:
	case EStep::WaitDispersedAgain:
	{
		SweepTime += DeltaTime;
		if (SweepTime > 1.0f)
		{
			FpsTime += DeltaTime;
			FpsFrames += 1;
		}

		LogAccumulator += DeltaTime;
		if (LogAccumulator >= 1.0f)
		{
			LogAccumulator = 0.0f;
			const FSchoolReading R = Read();
			UE_LOG(LogVibH2O, Display,
				TEXT("VibH2O: SCHOOL t=%5.1f s  synchronie=%.2f  ordre=%.2f  rotation=%+.2f  rayon_familles=%.0f cm  bulles=%d"),
				SweepTime, Sync, Order, R.Milling, R.FamilyRadius, R.Bodies);
		}

		// Each state is captured once it has held for a moment, measured on
		// the order the school is actually in - not on a guessed timetable.
		const bool bDispersed = Order < 0.02f;
		const bool bVortex = Order > 0.98f;
		StableTime = ((Current == EStep::WaitVortex) ? bVortex : bDispersed) ? StableTime + DeltaTime : 0.0f;

		if (StableTime >= 1.5f)
		{
			const FSchoolReading R = Read();
			const TCHAR* Name = Current == EStep::WaitDispersed ? TEXT("04_banc_disperse")
				: Current == EStep::WaitVortex ? TEXT("05_banc_vortex")
				: TEXT("06_banc_disperse_retour");
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL ETAT %s a t=%.1f s — synchronie %.2f, ordre %.2f, rotation %+.2f, rayon des familles %.0f cm."),
				Name, SweepTime, Sync, Order, R.Milling, R.FamilyRadius);
			Frame(false, Current == EStep::WaitVortex ? 0.25f : 0.55f, 1.0f);
			Shot(Name);
			// The sweep clock keeps running across the three waits.
			Enter(Current == EStep::WaitDispersed ? EStep::WaitVortex
				: Current == EStep::WaitVortex ? EStep::WaitDispersedAgain
				: EStep::Return);
		}
		else if (SweepTime > 90.0f)
		{
			UE_LOG(LogVibH2O, Error, TEXT("VibH2O: SCHOOL ECHEC — l'etat attendu n'est pas venu en 90 s (ordre %.2f)."), Order);
			Enter(EStep::Return);
		}
		break;
	}

	case EStep::Return:
		if (StepTime < 0.1f && FpsTime > 0.0f)
		{
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL images/s — salle seule (%d bulles) %.1f, banc complet (%d bulles) %.1f."),
				Stage->GetBubbleCount(), RoomFpsTime > 0.0f ? RoomFpsFrames / RoomFpsTime : 0.0f,
				Stage->GetBubbleCount() * Stage->GetFamilySize(), FpsFrames / FpsTime);
			FpsTime = 0.0f;
		}
		Stage->BlendAlpha = FMath::Clamp(1.0f - StepTime / 5.0f, 0.0f, 1.0f);
		if (StepTime >= 6.0f)
		{
			int32 Visible = 0;
			for (const AVibH2OBubbleActor* Daughter : Stage->GetDaughters())
			{
				Visible += Daughter->IsHidden() ? 0 : 1;
			}
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SCHOOL retour — %d filles encore visibles (%s)."),
				Visible, Visible == 0 ? TEXT("OK, refondues") : TEXT("ECHEC"));
			Frame(true, 0.6f, 1.0f);
			Shot(TEXT("07_retour_salle"));
			Enter(EStep::Done);
		}
		break;

	case EStep::Done:
		if (StepTime >= 1.0f && PendingShotFrames == 0)
		{
			Finish();
			return false;
		}
		break;
	}

	return true;
}

void FVibH2OSchoolSweep::Finish()
{
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: SchoolSweep termine — captures dans Saved/VibH2OSchool/."));
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

static FAutoConsoleCommandWithWorld GVibH2OSchoolSweepCommand(
	TEXT("VibH2O.SchoolSweep"),
	TEXT("Tableau 2 sur le simulateur : division, depart, balayage de synchronie (dispersion, vortex, dispersion), retour. Captures dans Saved/VibH2OSchool/, puis quitte."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&FVibH2OSchoolSweep::Start));

} // namespace VibH2OSchoolSweepInternal

#endif // !UE_BUILD_SHIPPING
