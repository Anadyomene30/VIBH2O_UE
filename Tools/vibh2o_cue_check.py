"""Sends cue inputs to Unreal - MIDI notes through a loopMIDI port, OSC
messages as Resolume would - and records which window had the focus.

Standard library only (ctypes for WinMM and the foreground window), Windows
only for MIDI and focus. Lets the cues be checked at get-in without touching
Resolume's setup, and proves the point that matters: Unreal reacts while
another application has the focus.

    python Tools/vibh2o_cue_check.py --list
    python Tools/vibh2o_cue_check.py --midi-port VibH2O --note 60
    python Tools/vibh2o_cue_check.py --osc /composition/columns/1/connect
    python Tools/vibh2o_cue_check.py --osc /Cue/Vortex
    python Tools/vibh2o_cue_check.py --midi-port VibH2O --scenario

--scenario plays the sequence of docs/CUES.md against the cue test map
(Tools/make_cue_test_map.py): note 60, column 1, column 2, /Cue/Vortex,
note 61, a couple of seconds apart.
"""

import argparse
import ctypes
import socket
import struct
import sys
import time

# --------------------------------------------------------------------- OSC


def osc_string(text: str) -> bytes:
    data = text.encode("utf-8") + b"\0"
    return data + b"\0" * (-len(data) % 4)


def osc_message(address: str, value=None) -> bytes:
    """Resolume sends an int 1 on a launch and an int 0 on the release."""
    if value is None:
        return osc_string(address) + osc_string(",")
    if isinstance(value, int):
        return osc_string(address) + osc_string(",i") + struct.pack(">i", value)
    return osc_string(address) + osc_string(",f") + struct.pack(">f", float(value))


# -------------------------------------------------------------------- Win32


def foreground_title() -> str:
    if sys.platform != "win32":
        return "?"
    user32 = ctypes.windll.user32
    window = user32.GetForegroundWindow()
    length = user32.GetWindowTextLengthW(window)
    buffer = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(window, buffer, length + 1)
    return buffer.value or "(sans titre)"


class MidiOutCaps(ctypes.Structure):
    _fields_ = [("wMid", ctypes.c_ushort), ("wPid", ctypes.c_ushort), ("vDriverVersion", ctypes.c_uint),
                ("szPname", ctypes.c_wchar * 32), ("wTechnology", ctypes.c_ushort), ("wVoices", ctypes.c_ushort),
                ("wNotes", ctypes.c_ushort), ("wChannelMask", ctypes.c_ushort), ("dwSupport", ctypes.c_uint)]


def midi_outputs() -> list[str]:
    winmm = ctypes.windll.winmm
    names = []
    for device in range(winmm.midiOutGetNumDevs()):
        caps = MidiOutCaps()
        winmm.midiOutGetDevCapsW(device, ctypes.byref(caps), ctypes.sizeof(caps))
        names.append(caps.szPname)
    return names


class MidiOut:
    def __init__(self, name_part: str):
        self.winmm = ctypes.windll.winmm
        self.handle = ctypes.c_void_p()
        for device, name in enumerate(midi_outputs()):
            if name_part.lower() in name.lower():
                result = self.winmm.midiOutOpen(ctypes.byref(self.handle), device, None, None, 0)
                if result != 0:
                    raise SystemExit(f"sortie MIDI '{name}' non ouverte (erreur WinMM {result})")
                self.name = name
                return
        raise SystemExit(f"aucune sortie MIDI ne contient '{name_part}' : {midi_outputs()}")

    def note(self, number: int, channel: int = 1, velocity: int = 127) -> None:
        status = 0x90 | ((channel - 1) & 0x0F)
        self.winmm.midiOutShortMsg(self.handle, status | (number << 8) | (velocity << 16))
        time.sleep(0.05)
        self.winmm.midiOutShortMsg(self.handle, (0x80 | ((channel - 1) & 0x0F)) | (number << 8))

    def close(self) -> None:
        self.winmm.midiOutClose(self.handle)


# --------------------------------------------------------------------- Main


def log(text: str) -> None:
    print(f"{time.strftime('%H:%M:%S')}  focus: {foreground_title()!r:40}  {text}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="liste les sorties MIDI")
    parser.add_argument("--midi-port", default="VibH2O", help="partie du nom du port loopMIDI")
    parser.add_argument("--note", type=int, help="envoie cette note MIDI")
    parser.add_argument("--channel", type=int, default=1)
    parser.add_argument("--osc", help="envoie cette adresse OSC (1 puis 0, comme Resolume)")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9002)
    parser.add_argument("--scenario", action="store_true", help="joue la suite de docs/CUES.md")
    parser.add_argument("--gap", type=float, default=2.0, help="secondes entre deux envois du scenario")
    args = parser.parse_args()

    if args.list:
        for name in midi_outputs():
            print(name)
        return 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send_osc(address: str) -> None:
        sock.sendto(osc_message(address, 1), (args.host, args.port))
        time.sleep(0.05)
        sock.sendto(osc_message(address, 0), (args.host, args.port))
        log(f"OSC {address} 1 puis 0 -> {args.host}:{args.port}")

    midi = None
    if args.note is not None or args.scenario:
        midi = MidiOut(args.midi_port)

    def send_note(number: int) -> None:
        midi.note(number, args.channel)
        log(f"MIDI note {number} canal {args.channel} -> {midi.name}")

    try:
        if args.note is not None:
            send_note(args.note)
        if args.osc:
            send_osc(args.osc)
        if args.scenario:
            steps = [
                lambda: send_note(60),
                lambda: send_osc("/composition/columns/1/connect"),
                lambda: send_osc("/composition/columns/2/connect"),
                lambda: send_osc("/Cue/Vortex"),
                lambda: send_note(61),
            ]
            for step in steps:
                step()
                time.sleep(args.gap)
    finally:
        if midi is not None:
            midi.close()
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
