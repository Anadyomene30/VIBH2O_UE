#!/usr/bin/env python3
"""Auto-test de l'outillage OSC — encodage, decodage, et boucle UDP reelle.

    python Tools/selftest_osc.py

Il verifie trois choses :

1. Que l'encodeur du simulateur et le decodeur de l'ecouteur sont d'accord,
   y compris sur les cas ou le remplissage a 4 octets se joue a un octet pres.
2. Que la trame de reference codee en dur dans le test C++
   (VibH2OOscParserTests.cpp) est bien celle que produit le simulateur. Si l'un
   des deux derive, ce test le dit avant qu'Unreal ne soit meme lance.
3. Qu'une salle envoyee en UDP sur la boucle locale se relit intacte, avec la
   bonne transposition.

Il ne demande ni Unreal, ni bibliotheque tierce.
"""

from __future__ import annotations

import socket
import struct
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from vibh2o_osc_sim import build_presets, osc_bundle, osc_message, seat_index  # noqa: E402
from vibh2o_osc_listen import decode_packet, segments  # noqa: E402

FAILURES: list[str] = []


def check(condition: bool, label: str) -> None:
    if condition:
        print(f"  ok    {label}")
    else:
        print(f"  ECHEC {label}")
        FAILURES.append(label)


def check_equal(actual, expected, label: str) -> None:
    check(actual == expected, f"{label} (obtenu {actual!r}, attendu {expected!r})" if actual != expected else label)


# ---------------------------------------------------------------------------


def test_reference_frame() -> None:
    print("\nTrame de reference, celle codee en dur dans le test C++")

    frame = osc_message("/RoomMapping/columns/", 5)
    expected = bytes([
        0x2F, 0x52, 0x6F, 0x6F, 0x6D, 0x4D, 0x61, 0x70, 0x70, 0x69, 0x6E, 0x67,
        0x2F, 0x63, 0x6F, 0x6C, 0x75, 0x6D, 0x6E, 0x73, 0x2F, 0x00, 0x00, 0x00,
        0x2C, 0x69, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x05,
    ])
    check_equal(len(frame), 32, "la trame fait 32 octets")
    check(frame == expected, "octet pour octet identique a celle du test C++")


def test_padding_edges() -> None:
    print("\nRemplissage a 4 octets — les cas qui se jouent a un octet pres")

    for address in ("/a/", "/ab/", "/abc/", "/abcd/", "/abcde/"):
        frame = osc_message(address, 1)
        check(len(frame) % 4 == 0, f"{address} produit une trame alignee")
        decoded = decode_packet(frame)
        check(len(decoded) == 1 and decoded[0][0] == address, f"{address} se relit intacte")

    # Une chaine dont la longueur est deja un multiple de 4 doit recevoir
    # QUATRE octets de remplissage, pas zero. C'est l'erreur classique.
    frame = osc_message("/x/", "abcd")
    decoded = decode_packet(frame)
    check(len(decoded) == 1 and decoded[0][1] == ["abcd"], "chaine de longueur multiple de 4")

    frame = osc_message("/x/", "abc")
    decoded = decode_packet(frame)
    check(len(decoded) == 1 and decoded[0][1] == ["abc"], "chaine de longueur 3")


def test_types_and_bundle() -> None:
    print("\nTypes et bundles")

    frame = osc_message("/Mix/1/", -17, 72.5, "HautGauche")
    decoded = decode_packet(frame)
    check(len(decoded) == 1, "message a trois arguments")
    if decoded:
        address, args = decoded[0]
        check_equal(address, "/Mix/1/", "adresse")
        check_equal(args[0], -17, "entier")
        check(abs(args[1] - 72.5) < 1e-6, "flottant")
        check_equal(args[2], "HautGauche", "chaine")

    bundle = osc_bundle([
        osc_message("/RoomMapping/1/", 11),
        osc_message("/BPM/11/", 68.25),
    ])
    decoded = decode_packet(bundle)
    check_equal(len(decoded), 2, "bundle a deux messages")
    if len(decoded) == 2:
        check_equal(decoded[0][0], "/RoomMapping/1/", "premier message du bundle")
        check_equal(decoded[1][0], "/BPM/11/", "deuxieme message du bundle")


def test_trailing_slash() -> None:
    print("\nPiege 1 — le slash final")

    check_equal(segments("/RoomMapping/columns/"), ["RoomMapping", "columns"], "segment vide ignore")
    check_equal(segments("/RoomMapping//12//"), ["RoomMapping", "12"], "slashs multiples ignores")
    check_equal(segments("/BPM/7/"), ["BPM", "7"], "adresse de donnee")


def test_seat_order() -> None:
    print("\nPiege 4 — la transposition")

    # La capture de reference du protocole : salle 5 x 5, les individus 1 a 5
    # apparaissent aux index 1, 6, 11, 16, 21. C'est ce que doit reproduire un
    # parcours en colonnes avec une numerotation visuelle en rangees.
    room = build_presets()["5x5"]
    observed = [seat_index(0, row, 5, 5, "column") for row in range(5)]
    # L'individu 1 est en (0,0), l'individu 6 en (0,1), etc.
    ids_at_first_indices = []
    for index in range(1, 6):
        zero = index - 1
        column, row = zero // room.rows, zero % room.rows
        ids_at_first_indices.append(room.individual_at(column, row))

    check_equal(ids_at_first_indices, [1, 6, 11, 16, 21], "la capture de reference du protocole est reproduite")
    check_equal(observed, [1, 2, 3, 4, 5], "les cinq premiers index descendent la premiere colonne")

    # Sur une salle non carree, les deux ordres doivent diverger.
    check(
        seat_index(1, 0, 7, 3, "column") != seat_index(1, 0, 7, 3, "row"),
        "sur une salle 7 x 3, les deux ordres divergent",
    )
    # Sur une salle carree, l'index differe aussi mais la salle reste
    # plausible a l'oeil : c'est bien pour cela que le 7 x 3 est indispensable.
    check_equal(seat_index(1, 0, 5, 5, "column"), 6, "5 x 5 column-major : (1,0) -> index 6")
    check_equal(seat_index(1, 0, 5, 5, "row"), 2, "5 x 5 row-major : (1,0) -> index 2")


def test_ales_room() -> None:
    print("\nLe vrai plan d'Ales")

    room = build_presets()["ales"]
    check_equal((room.columns, room.rows), (23, 4), "dimensions")
    check_equal(len(room.occupied_ids()), 84, "84 sieges occupes")

    for row in range(room.rows):
        check(room.individual_at(6, row) == 0, f"allee colonne 6, rangee {row} : siege vide")
        check(room.individual_at(16, row) == 0, f"allee colonne 16, rangee {row} : siege vide")

    check(room.individual_at(0, 0) != 0, "colonne 0 occupee")


def test_udp_loopback() -> None:
    print("\nBoucle UDP reelle — un plan de salle envoye puis relu")

    port = 9099
    received: list[tuple[str, list]] = []
    ready = threading.Event()
    stop = threading.Event()

    def listen() -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("127.0.0.1", port))
        sock.settimeout(0.2)
        ready.set()
        while not stop.is_set():
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                continue
            received.extend(decode_packet(data))
        sock.close()

    thread = threading.Thread(target=listen, daemon=True)
    thread.start()
    ready.wait(timeout=2.0)

    room = build_presets()["7x3"]
    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sender.sendto(osc_message("/RoomMapping/columns/", room.columns), ("127.0.0.1", port))
    sender.sendto(osc_message("/RoomMapping/rows/", room.rows), ("127.0.0.1", port))
    for row in range(room.rows):
        for column in range(room.columns):
            index = seat_index(column, row, room.columns, room.rows, "column")
            sender.sendto(
                osc_message(f"/RoomMapping/{index}/", room.individual_at(column, row)),
                ("127.0.0.1", port),
            )
    sender.sendto(osc_message("/BPM/1/", 72.5), ("127.0.0.1", port))
    sender.close()

    time.sleep(0.5)
    stop.set()
    thread.join(timeout=2.0)

    check_equal(len(received), 2 + room.columns * room.rows + 1, "tous les messages sont arrives")

    # On reconstruit la salle a partir de ce qui a ete recu, et on verifie que
    # chaque individu est revenu a sa place.
    seats: dict[int, int] = {}
    columns = rows = 0
    for address, args in received:
        parts = segments(address)
        if parts[0] != "RoomMapping":
            continue
        key = parts[1]
        if key == "columns":
            columns = args[0]
        elif key == "rows":
            rows = args[0]
        else:
            seats[int(key)] = args[0]

    check_equal((columns, rows), (7, 3), "dimensions relues")

    mismatches = 0
    for index, individual in seats.items():
        zero = index - 1
        column, row = zero // rows, zero % rows
        if room.individual_at(column, row) != individual:
            mismatches += 1
    check_equal(mismatches, 0, "chaque individu est revenu a sa place")


def main() -> int:
    print("Auto-test de l'outillage OSC VIBH2O")
    test_reference_frame()
    test_padding_edges()
    test_types_and_bundle()
    test_trailing_slash()
    test_seat_order()
    test_ales_room()
    test_udp_loopback()

    print()
    if FAILURES:
        print(f"{len(FAILURES)} echec(s) :")
        for failure in FAILURES:
            print(f"  - {failure}")
        return 1

    print("Tout est au vert.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
