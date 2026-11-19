#include "Cues/VibH2OMidiInput.h"

#include "Misc/ScopeLock.h"
#include "VibH2OModule.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <mmsystem.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	/** Room for a burst far beyond any controller; overflow drops the newest. */
	constexpr int32 RingCapacity = 4096;

#if PLATFORM_WINDOWS
	void CALLBACK MidiInCallback(HMIDIIN /*Handle*/, UINT Message, DWORD_PTR Instance, DWORD_PTR Param1, DWORD_PTR /*Param2*/)
	{
		// Short messages only: sysex (MIM_LONGDATA) is never a cue.
		if (Message == MIM_DATA && Instance != 0)
		{
			reinterpret_cast<FVibH2OMidiInput*>(Instance)->PushFromCallback(static_cast<uint32>(Param1));
		}
	}

	FString DescribeMmError(MMRESULT Result)
	{
		switch (Result)
		{
		case MMSYSERR_ALLOCATED:
			return TEXT("deja ouverte par une autre application (Resolume ?). Passer par un port loopMIDI, voir docs/CUES.md");
		case MMSYSERR_BADDEVICEID:
			return TEXT("peripherique disparu");
		case MMSYSERR_NOMEM:
			return TEXT("memoire insuffisante");
		default:
			return FString::Printf(TEXT("erreur WinMM %u"), static_cast<uint32>(Result));
		}
	}
#endif

	bool NameMatches(const FString& Name, const TArray<FString>& Filters)
	{
		for (const FString& Filter : Filters)
		{
			const FString Trimmed = Filter.TrimStartAndEnd();
			if (Trimmed == TEXT("*") || (!Trimmed.IsEmpty() && Name.Contains(Trimmed, ESearchCase::IgnoreCase)))
			{
				return true;
			}
		}
		return false;
	}
}

FVibH2OMidiInput::FVibH2OMidiInput()
{
	Ring.SetNumZeroed(RingCapacity);
}

FVibH2OMidiInput::~FVibH2OMidiInput()
{
	Shutdown();
}

TArray<FString> FVibH2OMidiInput::GetAvailableInputNames()
{
	TArray<FString> Names;
#if PLATFORM_WINDOWS
	const UINT DeviceCount = midiInGetNumDevs();
	for (UINT DeviceId = 0; DeviceId < DeviceCount; ++DeviceId)
	{
		MIDIINCAPSW Caps;
		if (midiInGetDevCapsW(DeviceId, &Caps, sizeof(Caps)) == MMSYSERR_NOERROR)
		{
			Names.Add(FString(Caps.szPname));
		}
		else
		{
			Names.Add(FString());
		}
	}
#endif
	return Names;
}

int32 FVibH2OMidiInput::Start(const TArray<FString>& NameFilters)
{
	Shutdown();

#if PLATFORM_WINDOWS
	const TArray<FString> Available = GetAvailableInputNames();
	for (int32 DeviceId = 0; DeviceId < Available.Num(); ++DeviceId)
	{
		const FString& Name = Available[DeviceId];
		if (Name.IsEmpty() || !NameMatches(Name, NameFilters))
		{
			continue;
		}

		HMIDIIN Handle = nullptr;
		MMRESULT Result = midiInOpen(&Handle, static_cast<UINT>(DeviceId),
			reinterpret_cast<DWORD_PTR>(&MidiInCallback), reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
		if (Result != MMSYSERR_NOERROR)
		{
			UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: entree MIDI '%s' non ouverte : %s."), *Name, *DescribeMmError(Result));
			continue;
		}

		Result = midiInStart(Handle);
		if (Result != MMSYSERR_NOERROR)
		{
			UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: entree MIDI '%s' ouverte mais pas demarree : %s."), *Name, *DescribeMmError(Result));
			midiInClose(Handle);
			continue;
		}

		Handles.Add(Handle);
		OpenNames.Add(Name);
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: entree MIDI '%s' ouverte pour les cues."), *Name);
	}
#endif

	return Handles.Num();
}

void FVibH2OMidiInput::Shutdown()
{
#if PLATFORM_WINDOWS
	// Once midiInClose returns, WinMM guarantees no further callback: the
	// object may then be destroyed safely.
	for (void* Raw : Handles)
	{
		HMIDIIN Handle = static_cast<HMIDIIN>(Raw);
		midiInStop(Handle);
		midiInReset(Handle);
		midiInClose(Handle);
	}
#endif
	Handles.Reset();
	OpenNames.Reset();

	FScopeLock ScopeLock(&Lock);
	Head = 0;
	Count = 0;
}

void FVibH2OMidiInput::PushFromCallback(uint32 PackedMessage)
{
	FScopeLock ScopeLock(&Lock);
	if (Count >= Ring.Num())
	{
		++Dropped;
		return;
	}
	Ring[(Head + Count) % Ring.Num()] = PackedMessage;
	++Count;
}

bool FVibH2OMidiInput::Dequeue(FVibH2OCueInput& OutInput)
{
	// Skips what is not a trigger - note offs, clock, aftertouch - so that the
	// caller only ever sees cue material.
	for (;;)
	{
		uint32 Packed = 0;
		{
			FScopeLock ScopeLock(&Lock);
			if (Count == 0)
			{
				return false;
			}
			Packed = Ring[Head];
			Head = (Head + 1) % Ring.Num();
			--Count;
		}
		if (VibH2OCues::DecodeMidiShortMessage(Packed, OutInput))
		{
			return true;
		}
	}
}

int32 FVibH2OMidiInput::GetDroppedCount() const
{
	FScopeLock ScopeLock(&Lock);
	return Dropped;
}
