#include "Cues/VibH2OBackgroundGuard.h"

#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "VibH2OModule.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
	void SetProcessPowerThrottling(bool bAllowThrottling)
	{
#if PLATFORM_WINDOWS
		// Looked up at run time: the engine compiles against an older Windows
		// target than the one that declares SetProcessInformation (Windows 8)
		// and its power throttling class (Windows 10 1709). The layout and
		// values below are those of processthreadsapi.h.
		struct FPowerThrottlingState
		{
			ULONG Version;
			ULONG ControlMask;
			ULONG StateMask;
		};
		constexpr int32 ProcessPowerThrottlingClass = 4;
		constexpr ULONG CurrentVersion = 1;
		constexpr ULONG ExecutionSpeed = 0x1;
		constexpr ULONG IgnoreTimerResolution = 0x4;

		using FSetProcessInformation = BOOL(WINAPI*)(HANDLE, int32, LPVOID, DWORD);
		HMODULE Kernel = GetModuleHandleW(L"kernel32.dll");
		const FSetProcessInformation SetInfo = Kernel
			? reinterpret_cast<FSetProcessInformation>(FPlatformProcess::GetDllExport(Kernel, TEXT("SetProcessInformation")))
			: nullptr;
		if (SetInfo == nullptr)
		{
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: bridage d'energie Windows absent de ce systeme, rien a faire."));
			return;
		}

		// A bit set in ControlMask and clear in StateMask means "never
		// throttle this". Both masks at zero hand the decision back to
		// Windows - setting both bits would force throttling instead.
		FPowerThrottlingState State;
		State.Version = CurrentVersion;
		State.ControlMask = bAllowThrottling ? 0 : (ExecutionSpeed | IgnoreTimerResolution);
		State.StateMask = 0;
		if (!SetInfo(GetCurrentProcess(), ProcessPowerThrottlingClass, &State, sizeof(State)))
		{
			UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: bridage d'energie Windows non modifie (erreur %u)."), GetLastError());
		}
#endif
	}
}

FVibH2OBackgroundGuard::~FVibH2OBackgroundGuard()
{
	Release();
}

void FVibH2OBackgroundGuard::Engage()
{
	if (bEngaged)
	{
		return;
	}
	bEngaged = true;

	if (IConsoleVariable* IdleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground")))
	{
		if (IdleVar->GetInt() != 0)
		{
			UE_LOG(LogVibH2O, Warning,
				TEXT("VibH2O: t.IdleWhenNotForeground valait %d ; remis a 0 pour que le rendu ne ralentisse pas derriere Resolume."),
				IdleVar->GetInt());
			IdleVar->Set(0, ECVF_SetByCode);
		}
	}

	SetProcessPowerThrottling(false);

#if WITH_EDITOR
	if (GEditor != nullptr)
	{
		UEditorEngine::FShouldDisableCPUThrottling Delegate =
			UEditorEngine::FShouldDisableCPUThrottling::CreateLambda([]() { return true; });
		EditorThrottleHandle = Delegate.GetHandle();
		GEditor->ShouldDisableCPUThrottlingDelegates.Add(MoveTemp(Delegate));

		if (GetDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground)
		{
			UE_LOG(LogVibH2O, Warning,
				TEXT("VibH2O: 'Use Less CPU when in Background' est actif dans les preferences de l'editeur. "
					 "Le CPU n'est plus bride pendant Play, mais les viewports de l'editeur cessent de se rafraichir "
					 "en arriere-plan. Le decocher (Editor Preferences > Performance) pour une representation depuis l'editeur."));
		}
	}
#endif

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: rendu maintenu a pleine vitesse quand Unreal est en arriere-plan."));
}

void FVibH2OBackgroundGuard::Release()
{
	if (!bEngaged)
	{
		return;
	}
	bEngaged = false;

	SetProcessPowerThrottling(true);

#if WITH_EDITOR
	if (GEditor != nullptr && EditorThrottleHandle.IsValid())
	{
		const FDelegateHandle Handle = EditorThrottleHandle;
		GEditor->ShouldDisableCPUThrottlingDelegates.RemoveAll(
			[Handle](const UEditorEngine::FShouldDisableCPUThrottling& Delegate) { return Delegate.GetHandle() == Handle; });
	}
	EditorThrottleHandle.Reset();
#endif
}

FString FVibH2OBackgroundGuard::DescribeState()
{
	int32 Idle = 0;
	if (IConsoleVariable* IdleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground")))
	{
		Idle = IdleVar->GetInt();
	}
	FString Text = FString::Printf(TEXT("Focus : %s | t.IdleWhenNotForeground=%d"), FApp::HasFocus() ? TEXT("Unreal") : TEXT("autre application"), Idle);
#if WITH_EDITOR
	if (GIsEditor)
	{
		Text += FString::Printf(TEXT(" | Use Less CPU in Background=%s"),
			GetDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground ? TEXT("OUI (a decocher)") : TEXT("non"));
	}
#endif
	return Text;
}
