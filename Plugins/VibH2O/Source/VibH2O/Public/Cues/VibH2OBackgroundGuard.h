// Keeps the show running at full speed while another application has the focus.
#pragma once

#include "CoreMinimal.h"

/**
 * During the show Resolume has the focus and Unreal sits behind it. Three
 * mechanisms can then slow Unreal down - and the cues would still arrive, but
 * the Spout feed would stutter:
 *
 *  - t.IdleWhenNotForeground (game): sleeps when the window loses the focus.
 *    0 by default; set back to 0 if a config has raised it.
 *  - "Use Less CPU when in Background" (editor, so Play In Editor): throttles
 *    the whole editor, and always does so once every window is minimised.
 *    Overridden through the editor's own opt-out delegate, for as long as a
 *    game session runs. The setting itself also stops the editor viewports
 *    from rendering in the background; that part only a warning can cover.
 *  - Windows power throttling (EcoQoS): Windows 11 may slow down a process
 *    whose windows are in the background, and ignore its timer resolution.
 *    The process opts out, which Windows documents as the way to do it.
 *
 * Engaged by the real subsystem only - the editor stand-in must leave the
 * editor's settings alone.
 */
class VIBH2O_API FVibH2OBackgroundGuard
{
public:
	~FVibH2OBackgroundGuard();

	void Engage();
	void Release();

	/** One line for the debug overlay. */
	static FString DescribeState();

private:
	bool bEngaged = false;

#if WITH_EDITOR
	FDelegateHandle EditorThrottleHandle;
#endif
};
