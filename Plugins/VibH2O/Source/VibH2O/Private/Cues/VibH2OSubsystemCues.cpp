// The cue half of the subsystem: MIDI and keyboard inputs, and the single
// stream of cue inputs the directors listen to. Kept apart from
// VibH2OSubsystem.cpp so that the room and data logic stays readable.
#include "VibH2OSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"

namespace VibH2OCueInputConsole
{
	static UVibH2OSubsystem* FindSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UVibH2OSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorld MidiListCommand(
		TEXT("VibH2O.Midi"),
		TEXT("Liste les entrees MIDI du PC et celles ouvertes pour les cues."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: entrees MIDI du PC : %s"),
				*FString::Join(UVibH2OSubsystem::GetAvailableMidiInputs(), TEXT(", ")));
			if (UVibH2OSubsystem* Subsystem = FindSubsystem(World))
			{
				UE_LOG(LogVibH2O, Display, TEXT("VibH2O: ouvertes pour les cues : %s"),
					*FString::Join(Subsystem->GetOpenMidiInputs(), TEXT(", ")));
			}
		}));

	static FAutoConsoleCommandWithWorld MidiRestartCommand(
		TEXT("VibH2O.MidiRestart"),
		TEXT("Rouvre les entrees MIDI des cues avec les reglages de projet courants."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			if (UVibH2OSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->StartMidiInput();
			}
		}));
}

void UVibH2OSubsystem::InitializeCueInputs()
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	if (Settings->bEnableMidiInput)
	{
		StartMidiInput();
	}
	if (Settings->bEnableKeyboardCues)
	{
		StartKeyboardCues();
	}
	if (Settings->bKeepRunningInBackground)
	{
		BackgroundGuard.Engage();
	}
}

// -------------------------------------------------------------------- MIDI

int32 UVibH2OSubsystem::StartMidiInput()
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	if (!MidiInput.IsValid())
	{
		MidiInput = MakeUnique<FVibH2OMidiInput>();
	}

	const int32 Opened = MidiInput->Start(Settings->MidiInputDevices);
	if (Opened == 0)
	{
		const TArray<FString> Available = GetAvailableMidiInputs();
		UE_LOG(LogVibH2O, Display,
			TEXT("VibH2O: aucune entree MIDI ouverte pour les cues (cherche : %s ; presentes : %s). Nouvel essai toutes les 5 s."),
			*FString::Join(Settings->MidiInputDevices, TEXT(", ")),
			Available.Num() > 0 ? *FString::Join(Available, TEXT(", ")) : TEXT("aucune"));
	}
	NextMidiRetryTime = FPlatformTime::Seconds() + 5.0;
	return Opened;
}

void UVibH2OSubsystem::StopMidiInput()
{
	if (MidiInput.IsValid())
	{
		MidiInput->Shutdown();
		MidiInput.Reset();
	}
}

TArray<FString> UVibH2OSubsystem::GetOpenMidiInputs() const
{
	return MidiInput.IsValid() ? MidiInput->GetOpenNames() : TArray<FString>();
}

TArray<FString> UVibH2OSubsystem::GetAvailableMidiInputs()
{
	return FVibH2OMidiInput::GetAvailableInputNames();
}

// ---------------------------------------------------------------- Keyboard

bool UVibH2OSubsystem::StartKeyboardCues()
{
	if (!KeyboardSink.IsValid())
	{
		KeyboardSink = MakeUnique<FVibH2OKeyboardSink>();
	}
	return KeyboardSink->Start();
}

void UVibH2OSubsystem::StopKeyboardCues()
{
	if (KeyboardSink.IsValid())
	{
		KeyboardSink->Shutdown();
		KeyboardSink.Reset();
	}
}

// ------------------------------------------------------------------ Stream

void UVibH2OSubsystem::InjectCueInput(const FVibH2OCueInput& Input)
{
	if (bIgnoreOscControl || bBroadcastingCueInput || !OnCueInput.IsBound())
	{
		return;
	}
	TGuardValue<bool> Guard(bBroadcastingCueInput, true);
	OnCueInput.Broadcast(Input);
}

void UVibH2OSubsystem::BroadcastOscCueInput(const FVibH2OOscMessage& Message)
{
	// Cheap exit first: this runs for every control message.
	if (!OnCueInput.IsBound() || bBroadcastingCueInput)
	{
		return;
	}

	FVibH2OCueInput Input;
	Input.Source = EVibH2OCueSource::Osc;
	Input.Address = Message.Address;
	if (const FVibH2OOscValue* First = Message.GetArg(0))
	{
		if (First->IsNumeric())
		{
			Input.Value = First->AsFloat();
			Input.bHasValue = true;
		}
	}
	InjectCueInput(Input);
}

void UVibH2OSubsystem::DrainCueInputs()
{
	FVibH2OCueInput Input;

	if (MidiInput.IsValid())
	{
		while (MidiInput->Dequeue(Input))
		{
			InjectCueInput(Input);
		}

		// A loopMIDI port created after start-up, or a controller plugged in
		// late: try again, quietly, until something opens.
		const UVibH2OSettings* Settings = UVibH2OSettings::Get();
		const double Now = FPlatformTime::Seconds();
		if (MidiInput->GetOpenCount() == 0 && Settings->bEnableMidiInput && Now >= NextMidiRetryTime)
		{
			NextMidiRetryTime = Now + 5.0;
			MidiInput->Start(Settings->MidiInputDevices);
		}
	}

	if (KeyboardSink.IsValid())
	{
		while (KeyboardSink->Dequeue(Input))
		{
			InjectCueInput(Input);
		}
	}
}

// ------------------------------------------------------------------- Learn

FString UVibH2OSubsystem::BeginInputCapture()
{
	++InputCaptureCount;
	if (!bEditorPreview)
	{
		return TEXT("entrees deja ouvertes");
	}

	// The stand-in never listens on its own: open everything for the time of
	// the capture.
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	TArray<FString> Parts;

	if (StartListening())
	{
		Parts.Add(FString::Printf(TEXT("OSC port %d"), Settings->ListenPort));
	}
	else
	{
		Parts.Add(FString::Printf(TEXT("OSC indisponible (%s)"), Receiver.IsValid() ? *Receiver->GetLastError() : TEXT("?")));
	}

	if (Settings->bEnableMidiInput)
	{
		StartMidiInput();
		const TArray<FString> Names = GetOpenMidiInputs();
		Parts.Add(Names.Num() > 0
			? FString::Printf(TEXT("MIDI %s"), *FString::Join(Names, TEXT(", ")))
			: TEXT("aucune entree MIDI"));
	}

	if (Settings->bEnableKeyboardCues && StartKeyboardCues())
	{
		Parts.Add(TEXT("clavier"));
	}

	return FString::Join(Parts, TEXT(" ; "));
}

void UVibH2OSubsystem::EndInputCapture()
{
	InputCaptureCount = FMath::Max(0, InputCaptureCount - 1);
	if (bEditorPreview && InputCaptureCount == 0)
	{
		StopListening();
		StopMidiInput();
		StopKeyboardCues();
	}
}
