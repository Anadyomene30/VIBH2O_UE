// Cue tests: the matching rules, the director against a real subsystem, and a
// real Level Sequence driven by cues.
//
// No device is opened here: MIDI bytes are decoded from their packed form,
// and inputs are injected where the devices would hand them over. What a
// device does on its own - WinMM, raw input - is checked on the machine, and
// recorded in docs/CUES.md.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Cues/VibH2OCueDirector.h"
#include "Cues/VibH2OCueTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/ScopeExit.h"
#include "MovieScene.h"
#include "Osc/VibH2OOscTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "VibH2OSettings.h"
#include "VibH2OSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VibH2OCueTestUtils
{
	static FVibH2OOscMessage MakeOsc(const FString& Address, float Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Value));
		return Message;
	}

	static FVibH2OCueInput MakeOscInput(const FString& Address, float Value)
	{
		FVibH2OCueInput Input;
		Input.Source = EVibH2OCueSource::Osc;
		Input.Address = Address;
		Input.Value = Value;
		Input.bHasValue = true;
		return Input;
	}

	/** WinMM's packing: status, then data 1, then data 2, low byte first. */
	static uint32 PackMidi(uint8 Status, uint8 Data1, uint8 Data2)
	{
		return static_cast<uint32>(Status) | (static_cast<uint32>(Data1) << 8) | (static_cast<uint32>(Data2) << 16);
	}

	/** A game world of its own, torn down at the end of the test. */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		explicit FTestWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, Name);
			if (World != nullptr)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
			}
		}

		~FTestWorld()
		{
			if (World != nullptr)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

// ---------------------------------------------------------------------------
// Pure rules: addresses, MIDI bytes, triggers, thresholds, learning.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OCueMatchingTest,
	"VibH2O.Cues.Correspondance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OCueMatchingTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OCueTestUtils;

	// --- Addresses: slashes and case do not matter, segments do.
	TestTrue(TEXT("Slash final ignore"), VibH2OCues::OscAddressesEqual(TEXT("/composition/columns/2/connect"), TEXT("/composition/columns/2/connect/")));
	TestTrue(TEXT("Casse ignoree"), VibH2OCues::OscAddressesEqual(TEXT("/Cue/Vortex"), TEXT("/cue/vortex")));
	TestFalse(TEXT("Une colonne n'est pas une autre"), VibH2OCues::OscAddressesEqual(TEXT("/composition/columns/2/connect"), TEXT("/composition/columns/3/connect")));
	TestFalse(TEXT("Un prefixe n'est pas une adresse"), VibH2OCues::OscAddressesEqual(TEXT("/composition/columns/2"), TEXT("/composition/columns/2/connect")));
	TestFalse(TEXT("Une adresse vide ne declenche jamais"), VibH2OCues::OscAddressesEqual(TEXT(""), TEXT("/")));

	// --- MIDI bytes as WinMM hands them over.
	FVibH2OCueInput Input;
	TestTrue(TEXT("Note On decodee"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0x90, 60, 100), Input));
	TestTrue(TEXT("Note On : source"), Input.Source == EVibH2OCueSource::MidiNote);
	TestEqual(TEXT("Note On : canal 1"), Input.Channel, 1);
	TestEqual(TEXT("Note On : numero"), Input.Number, 60);
	TestEqual(TEXT("Note On : velocite ramenee a 0..1"), Input.Value, 100.0f / 127.0f);

	TestFalse(TEXT("Note On a velocite 0 = Note Off, pas un declencheur"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0x90, 60, 0), Input));
	TestFalse(TEXT("Note Off ignoree"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0x80, 60, 64), Input));
	TestFalse(TEXT("Horloge MIDI ignoree"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0xF8, 0, 0), Input));
	TestFalse(TEXT("Octet de donnee seul rejete"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0x3C, 0, 0), Input));

	TestTrue(TEXT("CC decode"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0xB5, 7, 127), Input));
	TestTrue(TEXT("CC : source"), Input.Source == EVibH2OCueSource::MidiControlChange);
	TestEqual(TEXT("CC : canal 6"), Input.Channel, 6);
	TestEqual(TEXT("CC : valeur 127 -> 1"), Input.Value, 1.0f);

	TestTrue(TEXT("Program change decode"), VibH2OCues::DecodeMidiShortMessage(PackMidi(0xCF, 12, 0), Input));
	TestEqual(TEXT("Program change : canal 16"), Input.Channel, 16);
	TestFalse(TEXT("Program change : sans valeur"), Input.bHasValue);

	// --- Triggers.
	FVibH2OCue NoteCue;
	NoteCue.Source = EVibH2OCueSource::MidiNote;
	NoteCue.MidiNumber = 60;
	NoteCue.MidiChannel = 0;
	VibH2OCues::DecodeMidiShortMessage(PackMidi(0x93, 60, 90), Input);
	TestTrue(TEXT("Canal 0 accepte tous les canaux"), VibH2OCues::MatchesTrigger(NoteCue, Input));
	NoteCue.MidiChannel = 2;
	TestFalse(TEXT("Canal 2 refuse le canal 4"), VibH2OCues::MatchesTrigger(NoteCue, Input));
	NoteCue.MidiChannel = 4;
	TestTrue(TEXT("Canal 4 accepte le canal 4"), VibH2OCues::MatchesTrigger(NoteCue, Input));
	NoteCue.bEnabled = false;
	TestFalse(TEXT("Une cue inactive ne repond pas"), VibH2OCues::MatchesTrigger(NoteCue, Input));

	FVibH2OCue CcCue;
	CcCue.Source = EVibH2OCueSource::MidiControlChange;
	CcCue.MidiNumber = 60;
	TestFalse(TEXT("Une note n'est pas un CC de meme numero"), VibH2OCues::MatchesTrigger(CcCue, Input));

	// --- Thresholds: Resolume sends 1 on launch and 0 on release.
	FVibH2OCue OscCue;
	OscCue.Source = EVibH2OCueSource::Osc;
	OscCue.OscAddress = TEXT("/composition/columns/2/connect");
	TestTrue(TEXT("Appui (1) declenche"), VibH2OCues::PassesThreshold(OscCue, MakeOscInput(OscCue.OscAddress, 1.0f)));
	TestFalse(TEXT("Relache (0) ne declenche pas"), VibH2OCues::PassesThreshold(OscCue, MakeOscInput(OscCue.OscAddress, 0.0f)));
	OscCue.bContinuous = true;
	TestTrue(TEXT("En continu, tout passe"), VibH2OCues::PassesThreshold(OscCue, MakeOscInput(OscCue.OscAddress, 0.0f)));

	// --- Keys and modifiers.
	FVibH2OCue KeyCue;
	KeyCue.Source = EVibH2OCueSource::Key;
	KeyCue.Key = EKeys::One;
	FVibH2OCueInput KeyInput;
	KeyInput.Source = EVibH2OCueSource::Key;
	KeyInput.Key = EKeys::One;
	TestTrue(TEXT("Touche nue"), VibH2OCues::MatchesTrigger(KeyCue, KeyInput));
	KeyInput.bCtrl = true;
	TestFalse(TEXT("Ctrl+1 ne declenche pas la cue sur 1 seul"), VibH2OCues::MatchesTrigger(KeyCue, KeyInput));
	KeyCue.bRequireCtrl = true;
	TestTrue(TEXT("Ctrl+1 declenche la cue Ctrl+1"), VibH2OCues::MatchesTrigger(KeyCue, KeyInput));
	KeyInput.bCtrl = false;
	TestFalse(TEXT("1 seul ne declenche pas la cue Ctrl+1"), VibH2OCues::MatchesTrigger(KeyCue, KeyInput));

	// --- Learning copies the identity and nothing else.
	FVibH2OCue Learned;
	Learned.Action = EVibH2OCueAction::StopSequence;
	VibH2OCues::DecodeMidiShortMessage(PackMidi(0xB1, 21, 127), Input);
	VibH2OCues::ApplyLearnedInput(Learned, Input);
	TestTrue(TEXT("Appris : source"), Learned.Source == EVibH2OCueSource::MidiControlChange);
	TestEqual(TEXT("Appris : canal"), Learned.MidiChannel, 2);
	TestEqual(TEXT("Appris : numero"), Learned.MidiNumber, 21);
	TestTrue(TEXT("L'action n'est pas touchee"), Learned.Action == EVibH2OCueAction::StopSequence);
	TestTrue(TEXT("La cue apprise repond a son entree"), VibH2OCues::MatchesTrigger(Learned, Input));

	return true;
}

// ---------------------------------------------------------------------------
// The director against a real subsystem: OSC in, plugin routes out.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OCueDirectorTest,
	"VibH2O.Cues.Directeur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OCueDirectorTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OCueTestUtils;

	FTestWorld TestWorld(TEXT("VibH2O_TestCues"));
	if (TestWorld.World == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde de test."));
		return false;
	}

	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
	TStrongObjectPtr<UVibH2OSubsystem> Subsystem(NewObject<UVibH2OSubsystem>(GameInstance.Get()));
	AVibH2OCueDirector* Director = TestWorld.World->SpawnActor<AVibH2OCueDirector>();
	if (!TestNotNull(TEXT("Le directeur se cree"), Director) || !Subsystem.IsValid())
	{
		return false;
	}
	Director->MinRetriggerSeconds = 0.25f;

	// 0: a Resolume column launches the vortex.
	FVibH2OCue ColumnCue;
	ColumnCue.Name = TEXT("Colonne 2");
	ColumnCue.Source = EVibH2OCueSource::Osc;
	ColumnCue.OscAddress = TEXT("/composition/columns/2/connect");
	ColumnCue.Action = EVibH2OCueAction::SetBlendAlpha;
	ColumnCue.OutValue = 1.0f;
	Director->Cues.Add(ColumnCue);

	// 1: a fader drives the blend continuously.
	FVibH2OCue FaderCue;
	FaderCue.Name = TEXT("Fader");
	FaderCue.Source = EVibH2OCueSource::MidiControlChange;
	FaderCue.MidiNumber = 7;
	FaderCue.bContinuous = true;
	FaderCue.Action = EVibH2OCueAction::SendOsc;
	FaderCue.OutOscAddress = UVibH2OSettings::Get()->BlendPrefix;
	FaderCue.bUseInputValue = true;
	Director->Cues.Add(FaderCue);

	// 2: fired by name only.
	FVibH2OCue NamedCue;
	NamedCue.Name = TEXT("Vortex");
	NamedCue.Source = EVibH2OCueSource::MidiProgramChange;
	NamedCue.MidiNumber = 99;
	NamedCue.Action = EVibH2OCueAction::SetBlendAlpha;
	NamedCue.OutValue = 0.42f;
	Director->Cues.Add(NamedCue);

	// 3: a cue listening to /Blend and sending /Blend - must not loop.
	FVibH2OCue EchoCue;
	EchoCue.Name = TEXT("Echo");
	EchoCue.Source = EVibH2OCueSource::Osc;
	EchoCue.OscAddress = UVibH2OSettings::Get()->BlendPrefix;
	EchoCue.bContinuous = true;
	EchoCue.Action = EVibH2OCueAction::SendOsc;
	EchoCue.OutOscAddress = UVibH2OSettings::Get()->BlendPrefix;
	EchoCue.OutValue = 0.3f;
	EchoCue.bEnabled = false;
	Director->Cues.Add(EchoCue);

	Director->BindToSubsystem(Subsystem.Get());
	ON_SCOPE_EXIT
	{
		Director->UnbindFromSubsystem();
	};

	const FString Blend = UVibH2OSettings::Get()->BlendPrefix;

	// --- Through the whole OSC path: message -> cue input -> cue -> /Blend.
	Subsystem->InjectOscMessage(MakeOsc(TEXT("/composition/columns/2/connect/"), 1.0f));
	TestEqual(TEXT("Lancer la colonne 2 regle le blend a 1"), Subsystem->GetOscBlendAlpha(), 1.0f);

	Subsystem->InjectOscMessage(MakeOsc(Blend, 0.0f));
	Subsystem->InjectOscMessage(MakeOsc(TEXT("/composition/columns/2/connect/"), 0.0f));
	TestEqual(TEXT("Le relache de la colonne ne declenche rien"), Subsystem->GetOscBlendAlpha(), 0.0f);

	Subsystem->InjectOscMessage(MakeOsc(TEXT("/composition/columns/2/connect/"), 1.0f));
	TestEqual(TEXT("Un second appui dans l'anti-rebond est ignore"), Subsystem->GetOscBlendAlpha(), 0.0f);

	// --- A MIDI fader in continuous mode passes its value on.
	FVibH2OCueInput Fader;
	VibH2OCues::DecodeMidiShortMessage(0xB0 | (7 << 8) | (64 << 16), Fader);
	Subsystem->InjectCueInput(Fader);
	TestEqual(TEXT("Le fader transmet sa valeur"), Subsystem->GetOscBlendAlpha(), 64.0f / 127.0f);

	// --- By name, from OSC.
	Subsystem->InjectOscMessage(MakeOsc(TEXT("/Cue/vortex/"), 1.0f));
	TestEqual(TEXT("/Cue/<nom> declenche la cue par son nom"), Subsystem->GetOscBlendAlpha(), 0.42f);
	TestTrue(TEXT("FireCue par le nom"), Director->FireCue(TEXT("Colonne 2")));
	TestEqual(TEXT("FireCue execute l'action"), Subsystem->GetOscBlendAlpha(), 1.0f);
	TestFalse(TEXT("Une cue inconnue est signalee"), Director->FireCue(TEXT("N'existe pas")));

	// --- The echo cue must not call itself without end.
	Director->Cues[3].bEnabled = true;
	Subsystem->InjectOscMessage(MakeOsc(Blend, 0.9f));
	TestEqual(TEXT("Une cue qui renvoie son propre message ne boucle pas"), Subsystem->GetOscBlendAlpha(), 0.9f);
	Director->Cues[3].bEnabled = false;

	// --- Render protection cuts the cues too.
	Subsystem->bIgnoreOscControl = true;
	Subsystem->InjectCueInput(Fader);
	Subsystem->bIgnoreOscControl = false;
	TestEqual(TEXT("bIgnoreOscControl coupe aussi les cues"), Subsystem->GetOscBlendAlpha(), 0.9f);

	// --- Master switch.
	Director->bCuesActive = false;
	Subsystem->InjectCueInput(Fader);
	Director->bCuesActive = true;
	TestEqual(TEXT("Cues inactives : rien ne bouge"), Subsystem->GetOscBlendAlpha(), 0.9f);

	// --- Learn: a new cue is appended with the input's identity.
	const int32 CountBefore = Director->Cues.Num();
	Director->LearnTargetIndex = -1;
	FVibH2OCueInput Pad;
	VibH2OCues::DecodeMidiShortMessage(0x99 | (36 << 8) | (110 << 16), Pad);
	Director->ApplyLearnedInput(Pad);
	if (TestEqual(TEXT("Apprendre ajoute une cue"), Director->Cues.Num(), CountBefore + 1))
	{
		const FVibH2OCue& NewCue = Director->Cues.Last();
		TestTrue(TEXT("Cue apprise : note"), NewCue.Source == EVibH2OCueSource::MidiNote);
		TestEqual(TEXT("Cue apprise : canal 10"), NewCue.MidiChannel, 10);
		TestEqual(TEXT("Cue apprise : numero 36"), NewCue.MidiNumber, 36);
	}
	Director->LearnTargetIndex = 0;
	Director->ApplyLearnedInput(Pad);
	TestTrue(TEXT("Apprendre sur une cue existante la remplace"), Director->Cues[0].Source == EVibH2OCueSource::MidiNote);
	TestEqual(TEXT("... sans en ajouter"), Director->Cues.Num(), CountBefore + 1);

	return true;
}

// ---------------------------------------------------------------------------
// A real Level Sequence, driven by cues.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OCueSequenceTest,
	"VibH2O.Cues.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OCueSequenceTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OCueTestUtils;

	FTestWorld TestWorld(TEXT("VibH2O_TestCuesSequence"));
	if (TestWorld.World == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde de test."));
		return false;
	}

	// Ten seconds, with a marker at five.
	ULevelSequence* Sequence = NewObject<ULevelSequence>(GetTransientPackage(), NAME_None, RF_Transient);
	Sequence->Initialize();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	const FFrameRate TickRate = MovieScene->GetTickResolution();
	MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), TickRate.AsFrameNumber(10.0)));
	FMovieSceneMarkedFrame Marker(TickRate.AsFrameNumber(5.0));
	Marker.Label = TEXT("Acte2");
	MovieScene->AddMarkedFrame(Marker);

	ALevelSequenceActor* SequenceActor = TestWorld.World->SpawnActor<ALevelSequenceActor>();
	AVibH2OCueDirector* Director = TestWorld.World->SpawnActor<AVibH2OCueDirector>();
	if (!TestNotNull(TEXT("Acteur de sequence"), SequenceActor) || !TestNotNull(TEXT("Directeur"), Director))
	{
		return false;
	}
	SequenceActor->SetSequence(Sequence);
	ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
	if (!TestNotNull(TEXT("Lecteur de sequence"), Player) || !TestNotNull(TEXT("Sequence chargee"), Player->GetSequence()))
	{
		return false;
	}

	auto AddCue = [Director, SequenceActor](const TCHAR* Name, EVibH2OCueAction Action)
	{
		FVibH2OCue Cue;
		Cue.Name = Name;
		Cue.Source = EVibH2OCueSource::Osc;
		Cue.OscAddress = FString::Printf(TEXT("/test/%s"), Name);
		Cue.Action = Action;
		Cue.Sequence = SequenceActor;
		Director->Cues.Add(Cue);
		return Director->Cues.Num() - 1;
	};
	AddCue(TEXT("Lancer"), EVibH2OCueAction::PlaySequenceFromStart);
	AddCue(TEXT("Pause"), EVibH2OCueAction::PauseSequence);
	const int32 MarkerCue = AddCue(TEXT("Marqueur"), EVibH2OCueAction::JumpToMarker);
	Director->Cues[MarkerCue].MarkerName = TEXT("Acte2");
	Director->Cues[MarkerCue].bPlayAfterJump = false;
	const int32 TimeCue = AddCue(TEXT("Temps"), EVibH2OCueAction::JumpToTime);
	Director->Cues[TimeCue].TimeSeconds = 2.0f;
	Director->Cues[TimeCue].bPlayAfterJump = true;
	AddCue(TEXT("Arreter"), EVibH2OCueAction::StopSequence);

	Director->HandleCueInput(MakeOscInput(TEXT("/test/Lancer"), 1.0f));
	TestTrue(TEXT("La cue lance la sequence"), Player->IsPlaying());

	Director->HandleCueInput(MakeOscInput(TEXT("/test/Pause"), 1.0f));
	TestTrue(TEXT("La cue met en pause"), Player->IsPaused());

	Director->HandleCueInput(MakeOscInput(TEXT("/test/Marqueur"), 1.0f));
	TestEqual(TEXT("Saut au marqueur Acte2 (5 s)"), Player->GetCurrentTime().AsSeconds(), 5.0, 0.05);
	TestFalse(TEXT("Saut sans relancer : la sequence reste arretee"), Player->IsPlaying());

	Director->HandleCueInput(MakeOscInput(TEXT("/test/Temps"), 1.0f));
	TestEqual(TEXT("Saut a 2 s"), Player->GetCurrentTime().AsSeconds(), 2.0, 0.05);
	TestTrue(TEXT("Saut avec relance"), Player->IsPlaying());

	Director->HandleCueInput(MakeOscInput(TEXT("/test/Arreter"), 1.0f));
	TestFalse(TEXT("La cue arrete la sequence"), Player->IsPlaying());

	// A cue without a sequence warns rather than crashing (a warning, which
	// does not fail the test).
	FVibH2OCue Orphan;
	Orphan.Name = TEXT("Orpheline");
	Orphan.Action = EVibH2OCueAction::PlaySequence;
	Director->Cues.Add(Orphan);
	Director->FireCue(TEXT("Orpheline"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
