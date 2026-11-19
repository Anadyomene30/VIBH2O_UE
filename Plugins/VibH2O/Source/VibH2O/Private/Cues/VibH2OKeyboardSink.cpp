#include "Cues/VibH2OKeyboardSink.h"

#include "InputCoreTypes.h"
#include "VibH2OModule.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
	const TCHAR* const WindowClassName = TEXT("VibH2OKeyboardSink");

	/** Raw input allows one target window per device class: one sink at a time. */
	FVibH2OKeyboardSink* ActiveSink = nullptr;

	bool IsModifierKey(uint16 VirtualKey)
	{
		switch (VirtualKey)
		{
		case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
		case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
		case VK_MENU: case VK_LMENU: case VK_RMENU:
		case VK_LWIN: case VK_RWIN:
			return true;
		default:
			return false;
		}
	}

	LRESULT CALLBACK SinkWindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
	{
		if (Message == WM_INPUT && ActiveSink != nullptr)
		{
			RAWINPUT Raw;
			UINT Size = sizeof(Raw);
			if (GetRawInputData(reinterpret_cast<HRAWINPUT>(LParam), RID_INPUT, &Raw, &Size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1)
				&& Raw.header.dwType == RIM_TYPEKEYBOARD)
			{
				const RAWKEYBOARD& Keyboard = Raw.data.keyboard;
				// 0xFF is the fake key Windows inserts around some key sequences.
				if (Keyboard.VKey != 0 && Keyboard.VKey != 0xFF)
				{
					ActiveSink->HandleRawKey(Keyboard.VKey, (Keyboard.Flags & RI_KEY_BREAK) != 0);
				}
			}
		}
		// For WM_INPUT, DefWindowProc releases the raw input buffer.
		return DefWindowProcW(Window, Message, WParam, LParam);
	}
#endif
}

FVibH2OKeyboardSink::~FVibH2OKeyboardSink()
{
	Shutdown();
}

bool FVibH2OKeyboardSink::Start()
{
#if PLATFORM_WINDOWS
	if (WindowHandle != nullptr)
	{
		return true;
	}
	if (ActiveSink != nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: un autre lecteur de clavier est deja actif, celui-ci reste eteint."));
		return false;
	}

	HINSTANCE Instance = GetModuleHandleW(nullptr);

	WNDCLASSEXW WindowClass = {};
	WindowClass.cbSize = sizeof(WindowClass);
	WindowClass.lpfnWndProc = &SinkWindowProc;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = WindowClassName;
	// Already registered by an earlier start in this process: fine.
	if (RegisterClassExW(&WindowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: lecture du clavier impossible (classe de fenetre, erreur %u)."), GetLastError());
		return false;
	}

	HWND Window = CreateWindowExW(0, WindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, Instance, nullptr);
	if (Window == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: lecture du clavier impossible (fenetre, erreur %u)."), GetLastError());
		return false;
	}

	RAWINPUTDEVICE Device;
	Device.usUsagePage = 0x01; // Generic desktop
	Device.usUsage = 0x06;     // Keyboard
	// INPUTSINK: delivered even when another application has the focus.
	// Without NOLEGACY, the usual key messages still flow to every window.
	Device.dwFlags = RIDEV_INPUTSINK;
	Device.hwndTarget = Window;
	if (!RegisterRawInputDevices(&Device, 1, sizeof(Device)))
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: lecture du clavier impossible (raw input, erreur %u)."), GetLastError());
		DestroyWindow(Window);
		return false;
	}

	WindowHandle = Window;
	ActiveSink = this;
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: cues clavier actives, meme quand Unreal n'a pas le focus."));
	return true;
#else
	return false;
#endif
}

void FVibH2OKeyboardSink::Shutdown()
{
#if PLATFORM_WINDOWS
	if (WindowHandle != nullptr)
	{
		RAWINPUTDEVICE Device;
		Device.usUsagePage = 0x01;
		Device.usUsage = 0x06;
		Device.dwFlags = RIDEV_REMOVE;
		Device.hwndTarget = nullptr;
		RegisterRawInputDevices(&Device, 1, sizeof(Device));

		DestroyWindow(static_cast<HWND>(WindowHandle));
		WindowHandle = nullptr;
	}
	if (ActiveSink == this)
	{
		ActiveSink = nullptr;
	}
#endif
	HeldKeys.Reset();
	Pending.Reset();
}

void FVibH2OKeyboardSink::HandleRawKey(uint16 VirtualKey, bool bKeyUp)
{
#if PLATFORM_WINDOWS
	if (bKeyUp)
	{
		HeldKeys.Remove(VirtualKey);
		return;
	}

	// A held key repeats; only its first press counts. Modifiers are read as
	// state, never as triggers - otherwise learning Ctrl+1 would learn Ctrl.
	bool bAlreadyHeld = false;
	HeldKeys.Add(VirtualKey, &bAlreadyHeld);
	if (bAlreadyHeld || IsModifierKey(VirtualKey))
	{
		return;
	}

	const uint32 CharCode = MapVirtualKeyW(VirtualKey, MAPVK_VK_TO_CHAR);
	const FKey Key = FInputKeyManager::Get().GetKeyFromCodes(VirtualKey, CharCode);
	if (!Key.IsValid())
	{
		return;
	}

	FVibH2OCueInput Input;
	Input.Source = EVibH2OCueSource::Key;
	Input.Key = Key;
	// Asynchronous state: the modifiers as they are now, whichever window has
	// the focus.
	Input.bCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	Input.bShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	Input.bAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

	// Bounded, in case nobody drains for a while (a long load).
	if (Pending.Num() < 256)
	{
		Pending.Add(Input);
	}
#endif
}

bool FVibH2OKeyboardSink::Dequeue(FVibH2OCueInput& OutInput)
{
	if (Pending.Num() == 0)
	{
		return false;
	}
	OutInput = Pending[0];
	Pending.RemoveAt(0);
	return true;
}
