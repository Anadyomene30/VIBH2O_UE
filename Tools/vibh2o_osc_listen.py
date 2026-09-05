#!/usr/bin/env python3
"""Ecouteur OSC de diagnostic pour VIBH2O.

Il decode ce qui arrive sur un port UDP et reconstruit le plan de salle, ce qui
permet de valider le simulateur — et le patch Max — sans installer d'outil
tiers ni lancer Unreal.

    python vibh2o_osc_listen.py                  # ecoute 0.0.0.0:9002
    python vibh2o_osc_listen.py --raw            # affiche chaque message
    python vibh2o_osc_listen.py --order row      # relit la salle en RowMajor

Le decodeur est ecrit independamment de celui du plugin : s'ils sont d'accord
sur les memes trames, c'est un vrai recoupement.
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time

EMPTY_SEAT_IDS = {0, 999}


def _pad(offset: int) -> int:
    return offset + ((-offset) % 4)


def decode_message(data: bytes) -> tuple[str, list] | None:
    try:
        end = data.index(b"\x00")
        address = data[:end].decode("utf-8", errors="replace")
        offset = _pad(end + 1)
        if offset >= len(data):
            return address, []

        end = data.index(b"\x00", offset)
        tags = data[offset:end].decode("ascii", errors="replace")
        offset = _pad(end + 1)

        if not tags.startswith(","):
            return address, []

        args = []
        for tag in tags[1:]:
            if tag == "i":
                args.append(struct.unpack_from(">i", data, offset)[0])
                offset += 4
            elif tag == "f":
                args.append(struct.unpack_from(">f", data, offset)[0])
                offset += 4
            elif tag in "sS":
                end = data.index(b"\x00", offset)
                args.append(data[offset:end].decode("utf-8", errors="replace"))
                offset = _pad(end + 1)
            elif tag == "b":
                size = struct.unpack_from(">i", data, offset)[0]
                offset = _pad(offset + 4 + size)
                args.append(f"<blob {size}>")
            elif tag == "T":
                args.append(True)
            elif tag == "F":
                args.append(False)
            elif tag in "NI":
                args.append(None)
            elif tag in "htd":
                offset += 8
                args.append("<64 bits>")
            else:
                break
        return address, args
    except (ValueError, struct.error):
        return None


def decode_packet(data: bytes) -> list[tuple[str, list]]:
    """Decode un message isole ou un bundle."""
    if data.startswith(b"#bundle\x00"):
        out = []
        offset = 16
        while offset + 4 <= len(data):
            size = struct.unpack_from(">i", data, offset)[0]
            offset += 4
            if size <= 0 or offset + size > len(data):
                break
            out.extend(decode_packet(data[offset:offset + size]))
            offset += size
        return out
    decoded = decode_message(data)
    return [decoded] if decoded else []


def segments(address: str) -> list[str]:
    """Decoupe l'adresse en ignorant les segments vides — piege 1."""
    return [s for s in address.split("/") if s]


class RoomView:
    def __init__(self, order: str):
        self.order = order
        self.columns = 0
        self.rows = 0
        self.seats: dict[int, int] = {}
        self.dirty = False

    def place(self, index: int) -> tuple[int, int] | None:
        if self.columns <= 0 or self.rows <= 0:
            return None
        if index < 1 or index > self.columns * self.rows:
            return None
        zero = index - 1
        if self.order == "column":
            return zero // self.rows, zero % self.rows
        return zero % self.columns, zero // self.columns

    def render(self) -> str:
        if self.columns <= 0 or self.rows <= 0:
            return "  (dimensions inconnues)"
        grid = [["  .  " for _ in range(self.columns)] for _ in range(self.rows)]
        for index, individual in self.seats.items():
            place = self.place(index)
            if place is None:
                continue
            column, row = place
            grid[row][column] = "  .  " if individual in EMPTY_SEAT_IDS else f"{individual:^5d}"

        occupied = sum(1 for v in self.seats.values() if v not in EMPTY_SEAT_IDS)
        header = (
            f"  salle {self.columns} x {self.rows} — {occupied} occupes, "
            f"{len(self.seats) - occupied} vides — relue en {self.order}-major\n"
        )
        return header + "\n".join("  " + "".join(row) for row in grid)


def main() -> int:
    parser = argparse.ArgumentParser(description="Ecouteur OSC de diagnostic VIBH2O.")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9002)
    parser.add_argument("--order", default="column", choices=("column", "row"))
    parser.add_argument("--raw", action="store_true", help="Affiche chaque message recu.")
    parser.add_argument("--settle", type=float, default=0.1, help="Silence attendu avant de redessiner la salle, en secondes.")
    parser.add_argument("--mapping-prefix", default="/RoomMapping")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.host, args.port))
    sock.settimeout(0.25)

    room = RoomView(args.order)
    counts: dict[str, int] = {}
    last_mapping = 0.0
    started = time.monotonic()
    last_report = started
    total = 0

    print(f"Ecoute OSC sur {args.host}:{args.port} — Ctrl+C pour arreter.\n")

    prefix_segments = segments(args.mapping_prefix)

    try:
        while True:
            now = time.monotonic()
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                data = None

            if data:
                for address, arguments in decode_packet(data):
                    total += 1
                    if args.raw:
                        print(address, *arguments)

                    parts = segments(address)
                    root = "/" + (parts[0] if parts else "")
                    counts[root] = counts.get(root, 0) + 1

                    if parts[:len(prefix_segments)] == prefix_segments and len(parts) > len(prefix_segments):
                        key = parts[len(prefix_segments)]
                        value = int(arguments[0]) if arguments else 0
                        if key == "columns":
                            room.columns = value
                        elif key == "rows":
                            room.rows = value
                        elif key.isdigit():
                            room.seats[int(key)] = value
                        room.dirty = True
                        last_mapping = now

            # Le plan arrive en rafale sans marqueur de fin : on attend un
            # silence avant de le redessiner, exactement comme le plugin.
            if room.dirty and last_mapping > 0.0 and (now - last_mapping) >= args.settle:
                print("\n" + room.render() + "\n")
                room.dirty = False

            if now - last_report >= 5.0:
                summary = ", ".join(f"{k} x{v}" for k, v in sorted(counts.items()))
                rate = total / max(now - started, 0.001)
                print(f"  [{now - started:6.1f}s] {total} messages ({rate:.0f}/s) — {summary}")
                last_report = now

    except KeyboardInterrupt:
        print("\nArret.")
    finally:
        sock.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
