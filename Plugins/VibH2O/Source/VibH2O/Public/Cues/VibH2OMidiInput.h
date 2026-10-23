// MIDI input through WinMM, the system MIDI layer of Windows.
#pragma once

#include "CoreMinimal.h"
#include "Cues/VibH2OCueTypes.h"
#include "HAL/CriticalSection.h"

/**
 * Opens the MIDI inputs whose name matches, and hands their messages over to
 * the game thread.
 *
 * Why not Epic's MIDIDevice plugin: it is Beta, off by default, and only
 * polled from the game thread through PortMidi - which is WinMM underneath
 * anyway. Its API did hold between 5.5 and 5.8, but the plugin's status is
 * exactly the kind of thing that moves; WinMM has not changed since Windows 95.
 * Same reasoning as for the OSC parser: a hundred lines we own beat a
 * dependency we do not.
 *
 * Threading: WinMM calls back on a thread of its own. Like the OSC receiver,
 * that callback touches NO UObject and only writes to a buffer, which the game
 * thread drains. The buffer is preallocated and guarded by a critical section,
 * because WinMM forbids nearly every system call inside its callback -
 * EnterCriticalSection is one of the few it allows, a heap allocation is not
 * on the list.
 *
 * The input is read by the system, not by a window: it reaches Unreal whether
 * or not Unreal has the focus.
 */
class VIBH2O_API FVibH2OMidiInput
{
public:
	FVibH2OMidiInput();
	~FVibH2OMidiInput();

	/**
	 * Opens every input whose name contains one of the filters, case ignored.
	 * "*" opens every input - which, under classic WinMM, can take a hardware
	 * controller away from Resolume: see docs/CUES.md.
	 *
	 * @return the number of inputs opened. Each failure is logged by name.
	 */
	int32 Start(const TArray<FString>& NameFilters);

	/** Closes every input. Safe to call more than once. */
	void Shutdown();

	int32 GetOpenCount() const { return Handles.Num(); }
	const TArray<FString>& GetOpenNames() const { return OpenNames; }

	/** Pops the next decoded trigger. Game thread only. */
	bool Dequeue(FVibH2OCueInput& OutInput);

	/** Messages lost because the game thread fell behind. */
	int32 GetDroppedCount() const;

	/** Names of every MIDI input the system offers, open or not. */
	static TArray<FString> GetAvailableInputNames();

	/** Called by the WinMM callback. Not for anyone else. */
	void PushFromCallback(uint32 PackedMessage);

private:
	/** HMIDIIN, kept opaque so that no Windows header leaks out of here. */
	TArray<void*> Handles;
	TArray<FString> OpenNames;

	/** Fixed ring of raw packed messages, sized once and never reallocated. */
	mutable FCriticalSection Lock;
	TArray<uint32> Ring;
	int32 Head = 0;
	int32 Count = 0;
	int32 Dropped = 0;
};
