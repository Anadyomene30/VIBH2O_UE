#!/usr/bin/env python3
"""Simulateur OSC pour le plugin Unreal VIBH2O.

Il rejoue ce que le patch Max/MSP envoie a Unreal : un plan de salle en rafale,
puis un flux continu de BPM, d'excitation et de synchronie.

    python vibh2o_osc_sim.py                       # salle 5x5, envoi sur 127.0.0.1:9002
    python vibh2o_osc_sim.py --preset 7x3           # LE test de transposition
    python vibh2o_osc_sim.py --preset ales          # le vrai plan d'Ales, 23x4 avec allees
    python vibh2o_osc_sim.py --preset 176 --scenario wave
    python vibh2o_osc_sim.py --dry-run --duration 2 # affiche au lieu d'emettre

Pourquoi ce fichier et pas une extension de Scripts/OSC_SIMULATOR.py du depot
VibH2o : ce dernier simule les *capteurs en amont de Max*, sur des adresses
"/oh1/<hex>/bpm". Le plugin Unreal ecoute le maillon suivant, en aval de Max,
qui a une toute autre forme. Les conventions de ligne de commande et la
detection de broadcast sont reprises de lui.

Aucune dependance : la bibliotheque standard suffit. Les paquets OSC 1.0 sont
construits ici meme, ce qui en fait aussi une seconde implementation
independante de l'encodage — utile pour recouper le parseur C++.
"""

from __future__ import annotations

import argparse
import math
import random
import socket
import struct
import sys
import time
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Encodage OSC 1.0
# ---------------------------------------------------------------------------


def osc_string(text: str) -> bytes:
    """Chaine OSC : terminee par un zero, puis remplie jusqu'a un multiple de 4.

    Attention au cas ou la longueur est deja un multiple de 4 : le standard
    impose alors quatre octets de remplissage, pas zero.
    """
    raw = text.encode("utf-8") + b"\x00"
    padding = (-len(raw)) % 4
    return raw + b"\x00" * padding


def osc_message(address: str, *args) -> bytes:
    """Construit un message OSC. Types supportes : int, float, str."""
    tags = ","
    body = b""
    for arg in args:
        if isinstance(arg, bool):
            tags += "T" if arg else "F"
        elif isinstance(arg, int):
            tags += "i"
            body += struct.pack(">i", arg)
        elif isinstance(arg, float):
            tags += "f"
            body += struct.pack(">f", arg)
        elif isinstance(arg, str):
            tags += "s"
            body += osc_string(arg)
        else:
            raise TypeError(f"Type OSC non supporte : {type(arg)!r}")
    return osc_string(address) + osc_string(tags) + body


def osc_bundle(messages: list[bytes], timetag: int = 1) -> bytes:
    """Regroupe des messages dans un bundle OSC."""
    out = osc_string("#bundle") + struct.pack(">Q", timetag)
    for message in messages:
        out += struct.pack(">i", len(message)) + message
    return out


# ---------------------------------------------------------------------------
# Plans de salle
# ---------------------------------------------------------------------------

# Le vrai plan d'Ales, lu dans RoomMapping_Presets/ALES_FINAL.json du depot
# VibH2o : 23 colonnes, 4 rangees, et deux allees centrales (colonnes 6 et 16)
# ou les sieges valent 0. Non carre et troue : il exerce d'un seul coup le
# piege 3 (siege vide) et le piege 4 (transposition).
ALES_COLUMNS = 23
ALES_ROWS = 4
ALES_AISLE_COLUMNS = (6, 16)


@dataclass
class Room:
    """Un plan de salle : des dimensions, et un identifiant par place."""

    name: str
    columns: int
    rows: int
    # ids[(colonne, rangee)] -> identifiant d'individu, 0 pour un siege vide.
    ids: dict[tuple[int, int], int] = field(default_factory=dict)

    def individual_at(self, column: int, row: int) -> int:
        return self.ids.get((column, row), 0)

    def occupied_ids(self) -> list[int]:
        return sorted({v for v in self.ids.values() if v != 0})


def make_room(name: str, columns: int, rows: int, empty_cells=(), numbering="row") -> Room:
    """Construit un plan de salle avec une numerotation reguliere.

    numbering="row" numerote les individus en lecture horizontale, ce qui
    correspond a ce que montre l'interface de Max — et c'est justement cette
    difference avec l'index OSC qui constitue le piege 4.
    """
    room = Room(name=name, columns=columns, rows=rows)
    next_id = 1
    if numbering == "row":
        order = [(c, r) for r in range(rows) for c in range(columns)]
    else:
        order = [(c, r) for c in range(columns) for r in range(rows)]

    for column, row in order:
        if (column, row) in empty_cells or column in empty_cells:
            room.ids[(column, row)] = 0
        else:
            room.ids[(column, row)] = next_id
            next_id += 1
    return room


def build_presets() -> dict[str, Room]:
    presets: dict[str, Room] = {}

    # Cas nominal. Une grille carree : la transposition y est invisible, ce qui
    # est precisement pourquoi elle ne suffit pas.
    presets["5x5"] = make_room("5x5", 5, 5)

    # LE test. Non carre : si l'ordre de parcours est faux, la salle apparait
    # visiblement de travers.
    presets["7x3"] = make_room("7x3", 7, 3)

    # Trous dans la grille.
    presets["holes"] = make_room("holes", 6, 4, empty_cells={(1, 1), (2, 1), (4, 0), (0, 3), (5, 3)})

    # Le vrai plan d'Ales.
    ales = make_room("ales", ALES_COLUMNS, ALES_ROWS, empty_cells=set(ALES_AISLE_COLUMNS))
    presets["ales"] = ales

    # Tenue en charge a l'echelle reelle : 176 capteurs, comme le simulateur
    # historique du depot VibH2o.
    presets["176"] = make_room("176", 22, 8)

    # Au dela de la cible annoncee, pour voir ou ca casse.
    presets["200"] = make_room("200", 25, 8)

    return presets


# ---------------------------------------------------------------------------
# Index de siege — la transposition
# ---------------------------------------------------------------------------


def seat_index(column: int, row: int, columns: int, rows: int, order: str) -> int:
    """Index OSC, base 1.

    "column" reproduit la formule de Scripts/FormatRoomMapping.js cote Max :
    index = (x * rows + y) + 1, ou x est la colonne du matrixctrl.
    """
    if order == "column":
        return column * rows + row + 1
    return row * columns + column + 1


# ---------------------------------------------------------------------------
# Etat simule d'un individu
# ---------------------------------------------------------------------------


class Individual:
    """Un spectateur : un coeur qui derive lentement, une excitation, une synchronie."""

    def __init__(self, individual_id: int, rng: random.Random):
        self.id = individual_id
        self.rng = rng
        self.base_bpm = rng.uniform(58.0, 92.0)
        self.bpm = self.base_bpm
        self.excitation = rng.uniform(0.05, 0.25)
        self.synchrony = rng.uniform(0.2, 0.6)
        self.phase = rng.uniform(0.0, math.tau)
        self.muted_from: float | None = None

    def update(self, now: float, dt: float, excitation_target: float, synchrony_target: float) -> None:
        # Le coeur respire : une lente oscillation autour de sa base, plus un
        # peu de bruit. L'excitation tire le BPM vers le haut.
        self.phase += dt * 0.35
        breathing = math.sin(self.phase) * 3.0
        self.bpm = self.base_bpm + breathing + excitation_target * 28.0 + self.rng.gauss(0.0, 0.4)
        self.bpm = max(45.0, min(190.0, self.bpm))

        # Les valeurs cibles sont approchees progressivement : Max envoie des
        # grandeurs deja filtrees, pas des sauts.
        self.excitation += (excitation_target - self.excitation) * min(1.0, dt * 1.5)
        self.excitation = max(0.0, min(1.0, self.excitation + self.rng.gauss(0.0, 0.01)))

        self.synchrony += (synchrony_target - self.synchrony) * min(1.0, dt * 1.2)
        self.synchrony = max(0.0, min(1.0, self.synchrony + self.rng.gauss(0.0, 0.01)))


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------


def scenario_targets(scenario: str, elapsed: float, room: Room, column: int, row: int) -> tuple[float, float]:
    """Renvoie (excitation, synchronie) cibles pour une place, a un instant donne."""
    if scenario == "wave":
        # Une vague d'excitation qui traverse la salle de gauche a droite.
        position = column / max(room.columns - 1, 1)
        head = (elapsed * 0.18) % 1.4 - 0.2
        distance = abs(position - head)
        excitation = max(0.0, 1.0 - distance * 5.0)
        return excitation, 0.5

    if scenario == "sync-sweep":
        # La synchronie collective monte puis redescend : c'est ce balayage qui
        # fait respirer le vortex du tableau 2.
        synchrony = 0.5 - 0.5 * math.cos(elapsed * 0.25)
        return 0.3, synchrony

    if scenario == "peak":
        # Toute la salle monte ensemble, puis retombe.
        excitation = 0.5 - 0.5 * math.cos(elapsed * 0.4)
        return excitation, 0.4 + excitation * 0.5

    # nominal / dropout
    return 0.25 + 0.15 * math.sin(elapsed * 0.3 + column * 0.4 + row * 0.7), 0.55


# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------


class Sender:
    def __init__(self, host: str, port: int, dry_run: bool, verbose: bool):
        self.host = host
        self.port = port
        self.dry_run = dry_run
        self.verbose = verbose
        self.sent = 0
        self.sock: socket.socket | None = None
        if not dry_run:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            if host.endswith(".255"):
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    def send(self, address: str, *args) -> None:
        packet = osc_message(address, *args)
        self.sent += 1
        if self.dry_run or self.verbose:
            rendered = " ".join(
                f"{a:.3f}" if isinstance(a, float) else str(a) for a in args
            )
            print(f"{address} {rendered}")
        if self.sock is not None:
            self.sock.sendto(packet, (self.host, self.port))

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()


def send_room_mapping(sender: Sender, room: Room, prefix: str, order: str, settle_hint: float) -> None:
    """Emet le plan de salle en rafale, comme le fait Max.

    Aucun message ne signale la fin de l'envoi : c'est au recepteur d'attendre
    un silence. La rafale est emise aussi vite que possible, precisement pour
    exercer ce comportement.
    """
    sender.send(f"{prefix}/columns/", room.columns)
    sender.send(f"{prefix}/rows/", room.rows)

    for row in range(room.rows):
        for column in range(room.columns):
            index = seat_index(column, row, room.columns, room.rows, order)
            sender.send(f"{prefix}/{index}/", room.individual_at(column, row))

    occupied = len(room.occupied_ids())
    print(
        f"  plan envoye : {room.name} — {room.columns} colonnes x {room.rows} rangees, "
        f"{occupied} sieges occupes, {room.columns * room.rows - occupied} vides "
        f"(ordre {order}-major, silence attendu ~{settle_hint * 1000:.0f} ms)"
    )


def main() -> int:
    presets = build_presets()

    parser = argparse.ArgumentParser(
        description="Simulateur OSC VIBH2O — rejoue le flux Max vers Unreal.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--host", default="127.0.0.1", help="IP destinataire (defaut 127.0.0.1). Une adresse en .255 active le broadcast.")
    parser.add_argument("--port", type=int, default=9002, help="Port UDP (defaut 9002).")
    parser.add_argument("--preset", default="5x5", choices=sorted(presets.keys()), help="Plan de salle a simuler.")
    parser.add_argument("--columns", type=int, default=None, help="Force le nombre de colonnes (ignore le preset).")
    parser.add_argument("--rows", type=int, default=None, help="Force le nombre de rangees.")
    parser.add_argument(
        "--order",
        default="column",
        choices=("column", "row"),
        help="Sens de parcours de l'index OSC. 'column' reproduit FormatRoomMapping.js de Max.",
    )
    parser.add_argument(
        "--scenario",
        default="nominal",
        choices=("nominal", "wave", "sync-sweep", "peak", "dropout"),
        help="Comportement simule.",
    )
    parser.add_argument("--rate", type=float, default=10.0, help="Frequence d'envoi des donnees par individu, en Hz (defaut 10).")
    parser.add_argument("--mapping-interval", type=float, default=15.0, help="Intervalle entre deux envois du plan de salle, en secondes. 0 = une seule fois.")
    parser.add_argument("--duration", type=float, default=0.0, help="Duree totale en secondes. 0 = sans fin.")
    parser.add_argument("--dropout-after", type=float, default=8.0, help="Scenario dropout : instant ou le premier individu se tait.")
    parser.add_argument("--seed", type=int, default=1730, help="Graine aleatoire, pour des sessions reproductibles.")
    parser.add_argument("--dry-run", action="store_true", help="Affiche les messages sans rien emettre.")
    parser.add_argument("--verbose", action="store_true", help="Affiche chaque message emis.")
    parser.add_argument("--bpm-prefix", default="/BPM")
    parser.add_argument("--excitation-prefix", default="/SD")
    parser.add_argument("--synchrony-prefix", default="/Synchronie")
    parser.add_argument("--mapping-prefix", default="/RoomMapping")
    parser.add_argument("--list-presets", action="store_true", help="Decrit les plans disponibles et sort.")

    args = parser.parse_args()

    if args.list_presets:
        print("Plans de salle disponibles :\n")
        for name, room in sorted(presets.items()):
            occupied = len(room.occupied_ids())
            total = room.columns * room.rows
            print(f"  {name:8s} {room.columns:3d} x {room.rows:2d}  {occupied:4d} occupes / {total:4d} places")
        print(
            "\n  7x3   est le test de transposition : sur une grille carree, une salle"
            "\n        transposee ressemble a une salle correcte."
            "\n  ales  est le vrai plan de la salle d'Ales, avec ses deux allees centrales."
        )
        return 0

    room = presets[args.preset]
    if args.columns or args.rows:
        columns = args.columns or room.columns
        rows = args.rows or room.rows
        room = make_room(f"{columns}x{rows}", columns, rows)

    rng = random.Random(args.seed)
    individuals = {i: Individual(i, random.Random(args.seed + i)) for i in room.occupied_ids()}

    # Place de chaque individu, pour que les scenarios spatiaux sachent ou il est.
    seat_of: dict[int, tuple[int, int]] = {}
    for (column, row), individual_id in room.ids.items():
        if individual_id != 0:
            seat_of[individual_id] = (column, row)

    sender = Sender(args.host, args.port, args.dry_run, args.verbose)

    destination = "(rien emis)" if args.dry_run else f"{args.host}:{args.port}"
    print(f"Simulateur VIBH2O -> {destination}")
    print(f"  scenario   : {args.scenario}")
    print(f"  individus  : {len(individuals)}")
    print(f"  cadence    : {args.rate:.1f} Hz par individu, soit ~{args.rate * len(individuals) * 3:.0f} messages/s")
    print()

    send_room_mapping(sender, room, args.mapping_prefix, args.order, 0.1)
    if args.dry_run:
        # En mode sec on ne fait qu'un tour de donnees, sinon la sortie devient
        # illisible.
        elapsed = 0.0
        for individual in individuals.values():
            column, row = seat_of[individual.id]
            excitation, synchrony = scenario_targets(args.scenario, elapsed, room, column, row)
            individual.update(elapsed, 0.1, excitation, synchrony)
            sender.send(f"{args.bpm_prefix}/{individual.id}/", float(individual.bpm))
            sender.send(f"{args.excitation_prefix}/{individual.id}/", float(individual.excitation))
            sender.send(f"{args.synchrony_prefix}/{individual.id}/", float(individual.synchrony))
        print(f"\n{sender.sent} messages construits.")
        sender.close()
        return 0

    start = time.monotonic()
    last_mapping = start
    last_tick = start
    period = 1.0 / max(args.rate, 0.1)
    muted_id: int | None = None

    try:
        while True:
            now = time.monotonic()
            elapsed = now - start

            if args.duration > 0.0 and elapsed >= args.duration:
                break

            if args.mapping_interval > 0.0 and (now - last_mapping) >= args.mapping_interval:
                send_room_mapping(sender, room, args.mapping_prefix, args.order, 0.1)
                last_mapping = now

            # Scenario dropout : un individu cesse d'emettre. Le plugin doit le
            # basculer en muet au bout de son delai, et le sortir des moyennes.
            if args.scenario == "dropout" and muted_id is None and elapsed >= args.dropout_after:
                muted_id = sorted(individuals.keys())[0]
                print(f"  [{elapsed:5.1f}s] l'individu {muted_id} cesse d'emettre — il doit passer muet")

            dt = now - last_tick
            last_tick = now

            for individual in individuals.values():
                if individual.id == muted_id:
                    continue
                column, row = seat_of[individual.id]
                excitation, synchrony = scenario_targets(args.scenario, elapsed, room, column, row)
                individual.update(elapsed, dt, excitation, synchrony)

                sender.send(f"{args.bpm_prefix}/{individual.id}/", float(individual.bpm))
                sender.send(f"{args.excitation_prefix}/{individual.id}/", float(individual.excitation))
                sender.send(f"{args.synchrony_prefix}/{individual.id}/", float(individual.synchrony))

            sleep_for = period - (time.monotonic() - now)
            if sleep_for > 0:
                time.sleep(sleep_for)

    except KeyboardInterrupt:
        print("\nArret demande.")

    finally:
        sender.close()

    total = time.monotonic() - start
    print(f"\n{sender.sent} messages emis en {total:.1f} s ({sender.sent / max(total, 0.001):.0f}/s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
