#include "Cues/VibH2OCueDirector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Osc/VibH2OOscTypes.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"
#include "VibH2OSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "VibH2OCueDirector"

namespace VibH2OCueConsole
{
	/** "VibH2O.Cue <name>": fires a cue by hand, from the console. */
	static FAutoConsoleCommandWithWorldAndArgs FireCommand(
		TEXT("VibH2O.Cue"),
		TEXT("Déclenche une cue par son nom : VibH2O.Cue <nom>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || Args.Num() == 0)
			{
				UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: usage : VibH2O.Cue <nom>."));
				return;
			}
			const FString Name = FString::Join(Args, TEXT(" "));
			bool bFired = false;
			for (TActorIterator<AVibH2OCueDirector> It(World); It; ++It)
			{
				bFired |= It->FireCue(Name);
			}
			if (!bFired)
			{
				UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: aucune cue nommée '%s' dans ce niveau."), *Name);
			}
		}));
}

AVibH2OCueDirector::AVibH2OCueDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AVibH2OCueDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(this))
	{
		BindToSubsystem(Subsystem);
	}
	else
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: %s ne trouve pas le subsystem, aucune cue ne sera jouée."), *GetName());
	}
}

void AVibH2OCueDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void AVibH2OCueDirector::BeginDestroy()
{
	if (bLearning)
	{
		EndLearn(FString());
	}
	UnbindFromSubsystem();
	Super::BeginDestroy();
}

void AVibH2OCueDirector::BindToSubsystem(UVibH2OSubsystem* Subsystem)
{
	UnbindFromSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}
	BoundSubsystem = Subsystem;
	InputHandle = Subsystem->OnCueInput.AddUObject(this, &AVibH2OCueDirector::HandleCueInput);
}

void AVibH2OCueDirector::UnbindFromSubsystem()
{
	if (UVibH2OSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnCueInput.Remove(InputHandle);
	}
	InputHandle.Reset();
	BoundSubsystem.Reset();
}

// --------------------------------------------------------------- Execution

void AVibH2OCueDirector::HandleCueInput(const FVibH2OCueInput& Input)
{
	if (bLogInputs)
	{
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: entrée de cue : %s"), *Input.Describe());
	}
	if (!bCuesActive)
	{
		return;
	}

	// "/Cue/<name>" fires a cue by name, whatever its trigger. This is the
	// route for Resolume's custom OSC addresses: nothing to learn.
	const FString& CuePrefix = UVibH2OSettings::Get()->CuePrefix;
	if (Input.Source == EVibH2OCueSource::Osc && !CuePrefix.IsEmpty())
	{
		FVibH2OOscMessage AsMessage(Input.Address);
		TArray<FString> Segments;
		if (AsMessage.GetSegmentsAfterPrefix(CuePrefix, Segments) && Segments.Num() > 0)
		{
			// Same threshold as a learned cue: Resolume's button release (0)
			// must not fire the cue a second time.
			if (!Input.bHasValue || Input.Value >= 0.5f)
			{
				FireCue(FString::Join(Segments, TEXT("/")), Input.bHasValue ? Input.Value : 1.0f);
			}
			return;
		}
	}

	const double Now = FPlatformTime::Seconds();
	LastFireTimes.SetNumZeroed(Cues.Num());

	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		const FVibH2OCue& Cue = Cues[Index];
		if (!VibH2OCues::MatchesTrigger(Cue, Input) || !VibH2OCues::PassesThreshold(Cue, Input))
		{
			continue;
		}
		if (!Cue.bContinuous)
		{
			if (LastFireTimes[Index] > 0.0 && (Now - LastFireTimes[Index]) < MinRetriggerSeconds)
			{
				continue;
			}
			LastFireTimes[Index] = Now;
		}
		ExecuteCue(Index, Input.bHasValue ? Input.Value : 1.0f);
	}
}

bool AVibH2OCueDirector::FireCue(const FString& CueName, float Value)
{
	bool bFound = false;
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		if (Cues[Index].bEnabled && Cues[Index].Name.Equals(CueName, ESearchCase::IgnoreCase))
		{
			ExecuteCue(Index, Value);
			bFound = true;
		}
	}
	if (!bFound)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: cue '%s' inconnue ou inactive."), *CueName);
	}
	return bFound;
}

void AVibH2OCueDirector::ExecuteCue(int32 CueIndex, float InputValue)
{
	// A copy, not a reference: the OSC it sends and OnCueFired reach
	// Blueprints, and one that edits Cues would leave a reference dangling.
	const FVibH2OCue Cue = Cues[CueIndex];
	const UEnum* ActionEnum = StaticEnum<EVibH2OCueAction>();
	const FString ActionName = ActionEnum ? ActionEnum->GetDisplayNameTextByValue(static_cast<int64>(Cue.Action)).ToString() : FString();

	// Continuous cues fire on every fader move: Verbose, not to drown the log.
	if (Cue.bContinuous)
	{
		UE_LOG(LogVibH2O, Verbose, TEXT("VibH2O: cue '%s' -> %s (%g)."), *Cue.Name, *ActionName, InputValue);
	}
	else
	{
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: cue '%s' -> %s."), *Cue.Name, *ActionName);
	}

	const float OutValue = Cue.bUseInputValue ? InputValue : Cue.OutValue;

	switch (Cue.Action)
	{
	case EVibH2OCueAction::PlaySequence:
	case EVibH2OCueAction::PlaySequenceFromStart:
	case EVibH2OCueAction::PauseSequence:
	case EVibH2OCueAction::StopSequence:
	case EVibH2OCueAction::JumpToMarker:
	case EVibH2OCueAction::JumpToTime:
		ExecuteSequenceAction(Cue);
		break;

	case EVibH2OCueAction::SetBlendAlpha:
	case EVibH2OCueAction::SendOsc:
	case EVibH2OCueAction::ConduiteGo:
	case EVibH2OCueAction::ConduitePrevious:
	case EVibH2OCueAction::ConduiteNext:
	{
		// Through the subsystem's own routes: the stage follows /Blend, the
		// attractors follow /Attractor, the desk follows /Regie - the cue
		// adds no second path.
		const FString Regie = UVibH2OSettings::Get()->RegiePrefix;
		const FString Address = Cue.Action == EVibH2OCueAction::SetBlendAlpha ? UVibH2OSettings::Get()->BlendPrefix
			: Cue.Action == EVibH2OCueAction::ConduiteGo ? Regie + TEXT("/Conduite/Go")
			: Cue.Action == EVibH2OCueAction::ConduitePrevious ? Regie + TEXT("/Conduite/Precedent")
			: Cue.Action == EVibH2OCueAction::ConduiteNext ? Regie + TEXT("/Conduite/Suivant")
			: Cue.OutOscAddress;
		UVibH2OSubsystem* Subsystem = BoundSubsystem.IsValid() ? BoundSubsystem.Get() : UVibH2OSubsystem::Get(this);
		if (Subsystem == nullptr || Address.IsEmpty())
		{
			UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: cue '%s' : pas de subsystem ou adresse vide."), *Cue.Name);
			break;
		}
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeFloat(OutValue));
		Subsystem->InjectOscMessage(Message);
		break;
	}

	case EVibH2OCueAction::BlueprintEvent:
		break;
	}

	OnCueFired.Broadcast(Cue.Name, InputValue);
}

void AVibH2OCueDirector::ExecuteSequenceAction(const FVibH2OCue& Cue)
{
	ALevelSequenceActor* SequenceActor = Cue.Sequence;
	ULevelSequencePlayer* Player = SequenceActor ? SequenceActor->GetSequencePlayer() : nullptr;
	if (Player == nullptr || Player->GetSequence() == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: cue '%s' : aucune Level Sequence assignée ou initialisée."), *Cue.Name);
		return;
	}

	switch (Cue.Action)
	{
	case EVibH2OCueAction::PlaySequence:
		Player->Play();
		break;

	case EVibH2OCueAction::PlaySequenceFromStart:
		// A jump, not a scrub: the events between here and the start are not
		// replayed.
		Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(Player->GetStartTime().Time, EUpdatePositionMethod::Jump));
		Player->Play();
		break;

	case EVibH2OCueAction::PauseSequence:
		Player->Pause();
		break;

	case EVibH2OCueAction::StopSequence:
		Player->Stop();
		break;

	case EVibH2OCueAction::JumpToMarker:
	case EVibH2OCueAction::JumpToTime:
	{
		const FMovieSceneSequencePlaybackParams Params = Cue.Action == EVibH2OCueAction::JumpToMarker
			? FMovieSceneSequencePlaybackParams(Cue.MarkerName, EUpdatePositionMethod::Jump)
			: FMovieSceneSequencePlaybackParams(Cue.TimeSeconds, EUpdatePositionMethod::Jump);
		Player->SetPlaybackPosition(Params);
		if (Cue.bPlayAfterJump)
		{
			Player->Play();
		}
		else
		{
			Player->Pause();
		}
		break;
	}

	default:
		break;
	}

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O:    séquence '%s' %s à %.2f s."),
		*Player->GetSequence()->GetName(),
		Player->IsPlaying() ? TEXT("en lecture") : (Player->IsPaused() ? TEXT("en pause") : TEXT("arrêtée")),
		Player->GetCurrentTime().AsSeconds());
}

// -------------------------------------------------------------------- Test

TArray<FString> AVibH2OCueDirector::GetCueNames() const
{
	TArray<FString> Names;
	for (const FVibH2OCue& Cue : Cues)
	{
		if (!Cue.Name.IsEmpty())
		{
			Names.AddUnique(Cue.Name);
		}
	}
	return Names;
}

void AVibH2OCueDirector::FireTestCue()
{
	if (TestCueName.IsEmpty())
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: choisir d'abord la cue à tester."));
		return;
	}

	AVibH2OCueDirector* Target = this;
#if WITH_EDITOR
	// The button is most often pressed on the level's director, not on its
	// copy in the running Play: hand the test to that copy.
	const UWorld* OwnWorld = GetWorld();
	if (OwnWorld != nullptr && !OwnWorld->IsGameWorld())
	{
		Target = Cast<AVibH2OCueDirector>(EditorUtilities::GetSimWorldCounterpartActor(this));
	}
#endif

	const UWorld* TargetWorld = Target != nullptr ? Target->GetWorld() : nullptr;
	if (TargetWorld == nullptr || !TargetWorld->IsGameWorld())
	{
		// A Level Sequence does not play in the editor world: firing here
		// would look like a broken cue.
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: Lancer Play pour tester la cue '%s' : hors Play, rien ne joue."), *TestCueName);
		return;
	}
	Target->FireCue(TestCueName);
}

// ------------------------------------------------------------------- Learn

void AVibH2OCueDirector::ApplyLearnedInput(const FVibH2OCueInput& Input)
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(LOCTEXT("LearnCue", "Apprendre une cue"));
	Modify();
#endif

	int32 Index = LearnTargetIndex;
	if (!Cues.IsValidIndex(Index))
	{
		FVibH2OCue NewCue;
		NewCue.Name = FString::Printf(TEXT("Cue %d"), Cues.Num() + 1);
		Index = Cues.Add(NewCue);
	}
	VibH2OCues::ApplyLearnedInput(Cues[Index], Input);

	LearnStatus = FString::Printf(TEXT("Cue %d (%s) : %s"), Index, *Cues[Index].Name, *Input.Describe());
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: appris -> %s"), *LearnStatus);

#if WITH_EDITOR
	// Refreshes the Details panel, and marks the level as modified.
	PostEditChange();
	MarkPackageDirty();
#endif
}

void AVibH2OCueDirector::HandleLearnInput(const FVibH2OCueInput& Input)
{
	if (!bLearning)
	{
		return;
	}
	// A release - Resolume's 0 after a launch, a pad's CC 0 - is never what
	// the operator meant to teach.
	if (Input.bHasValue && Input.Value <= 0.0f)
	{
		return;
	}
	ApplyLearnedInput(Input);
	EndLearn(LearnStatus);
}

void AVibH2OCueDirector::EndLearn(const FString& Status)
{
	bLearning = false;
	if (LearnTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LearnTickerHandle);
		LearnTickerHandle.Reset();
	}
#if WITH_EDITOR
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPieHandle);
	PreBeginPieHandle.Reset();
#endif
	if (UVibH2OSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnCueInput.Remove(InputHandle);
		Subsystem->EndInputCapture();
	}
	InputHandle.Reset();
	BoundSubsystem.Reset();
	if (!Status.IsEmpty())
	{
		LearnStatus = Status;
	}
}

bool AVibH2OCueDirector::TickLearn(float DeltaTime)
{
	if (!bLearning)
	{
		return false;
	}
#if WITH_EDITOR
	// The editor stand-in is ticked by its users; here, by the capture.
	if (UVibH2OSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->TickEditorPreview(DeltaTime);
	}
#endif
	// The input may have ended the capture during that tick.
	if (bLearning && FPlatformTime::Seconds() > LearnDeadline)
	{
		EndLearn(TEXT("Rien reçu en 30 s. Vérifier la sortie OSC de Resolume, le port loopMIDI, ou le réglage des entrées MIDI."));
	}
	return bLearning;
}

#if WITH_EDITOR
void AVibH2OCueDirector::Learn()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->WorldType != EWorldType::Editor)
	{
		LearnStatus = TEXT("L'apprentissage se fait dans l'éditeur, hors Play.");
		return;
	}
	if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
	{
		LearnStatus = TEXT("Arrêter Play d'abord : la partie en cours tient déjà le port OSC et les entrées.");
		return;
	}

	if (bLearning)
	{
		EndLearn(FString());
	}

	UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::GetEditorPreview(World);
	if (Subsystem == nullptr)
	{
		LearnStatus = TEXT("Subsystem de prévisualisation indisponible.");
		return;
	}

	const FString Report = Subsystem->BeginInputCapture();
	BoundSubsystem = Subsystem;
	InputHandle = Subsystem->OnCueInput.AddUObject(this, &AVibH2OCueDirector::HandleLearnInput);
	bLearning = true;
	LearnDeadline = FPlatformTime::Seconds() + 30.0;
	LearnTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &AVibH2OCueDirector::TickLearn));
	// Play would find the OSC port held by the capture: let go first.
	PreBeginPieHandle = FEditorDelegates::PreBeginPIE.AddWeakLambda(this, [this](bool /*bIsSimulating*/)
	{
		if (bLearning)
		{
			EndLearn(TEXT("Apprentissage interrompu par Play."));
		}
	});

	const FString Target = Cues.IsValidIndex(LearnTargetIndex)
		? FString::Printf(TEXT("cue %d"), LearnTargetIndex)
		: TEXT("une nouvelle cue");
	LearnStatus = FString::Printf(TEXT("En écoute pour %s (30 s) - %s"), *Target, *Report);
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: %s"), *LearnStatus);
}

void AVibH2OCueDirector::CancelLearn()
{
	if (bLearning)
	{
		EndLearn(TEXT("Apprentissage annulé."));
	}
}

void AVibH2OCueDirector::ListMidiInputs()
{
	const TArray<FString> Names = UVibH2OSubsystem::GetAvailableMidiInputs();
	LearnStatus = Names.Num() > 0
		? FString::Printf(TEXT("Entrées MIDI : %s"), *FString::Join(Names, TEXT(", ")))
		: TEXT("Aucune entrée MIDI sur ce PC. Créer un port dans loopMIDI.");
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: %s"), *LearnStatus);
}
#endif

#undef LOCTEXT_NAMESPACE
