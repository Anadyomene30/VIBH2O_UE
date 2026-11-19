// Keyboard reading that does not need Unreal's window to have the focus.
#pragma once

#include "CoreMinimal.h"
#include "Cues/VibH2OCueTypes.h"

/**
 * Reads the keyboard through Windows Raw Input, in "input sink" mode.
 *
 * Unreal's own input only sees keys while its window has the focus. With
 * RIDEV_INPUTSINK, Windows also hands key presses meant for another
 * application to a window of ours - WITHOUT taking them away: Resolume still
 * receives the very same key. That is the point (one key can drive both) and
 * the risk (typing a clip name in Resolume fires the cues too). Hence off by
 * default, and modifiers on the cues - see docs/CUES.md.
 *
 * The other Windows route, RegisterHotKey, was ruled out: it steals the key
 * from every other application, Resolume included.
 *
 * The sink owns a hidden message-only window, created on the game thread. The
 * engine pumps that thread's messages every frame, so WM_INPUT arrives there
 * with no thread of our own: the sink only stacks key presses, the subsystem
 * drains them from its tick. The engine registers raw input for the mouse
 * only, never for the keyboard, so the two registrations do not collide.
 */
class VIBH2O_API FVibH2OKeyboardSink
{
public:
	FVibH2OKeyboardSink() = default;
	~FVibH2OKeyboardSink();

	/** Creates the window and registers for the keyboard. Game thread only. */
	bool Start();

	void Shutdown();

	bool IsRunning() const { return WindowHandle != nullptr; }

	/** Pops the next key press. Game thread only. */
	bool Dequeue(FVibH2OCueInput& OutInput);

	/** Called from the window procedure. Not for anyone else. */
	void HandleRawKey(uint16 VirtualKey, bool bKeyUp);

private:
	/** HWND, kept opaque. */
	void* WindowHandle = nullptr;

	/** Keys held down, to swallow the auto-repeat of a held key. */
	TSet<uint16> HeldKeys;

	TArray<FVibH2OCueInput> Pending;
};
