#!/usr/bin/env python3
"""Rejoue un enregistrement de capteurs vers Unreal, avec le vrai plan de salle.

Pour repeter sans Max ni public : les BPM enregistres sont renvoyes a leur
rythme (ou plus vite), chaque capteur a sa place dans la salle.

    python vibh2o_replay.py SESSION_ALES.txt --plan ALES_FINAL.json
    python vibh2o_replay.py SESSION_ALES.txt --plan ALES_FINAL.json --speed 4 --start 600

Ce qu'il faut savoir :
  * L'enregistrement donne, par ligne : "n, /<capteur>/ <bpm> <horodatage ms>.;"
  * Le plan (RoomMapping_Presets du depot VibH2o) donne "colonne rangee" -> capteur.
  * L'excitation et la synchronie sont normalement calculees par Max. Ici elles
    sont APPROCHEES pour la repetition : excitation = ecart du BPM lisse au BPM
    de repos de la personne (mediane des premieres minutes), rapporte a
    --excitation-range ; synchronie = proximite de la tendance de la personne a
    celle de la salle. L'analyse de Max reste la reference.
  * Les enregistrements sont des donnees personnelles : ne jamais les copier
    dans ce depot.
"""
from __future__ import annotations

import argparse
import json
import math
import statistics
import sys
import time
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from vibh2o_osc_sim import Room, Sender, send_room_mapping  # noqa: E402


def read_record(path: Path) -> list[tuple[float, int, float]]:
    """(secondes depuis le debut, capteur, bpm), dans l'ordre du temps."""
    events = []
    with path.open(encoding="utf-8", errors="replace") as handle:
        for line in handle:
            parts = line.replace(";", " ").replace(",", " ").split()
            if len(parts) < 4 or not parts[1].startswith("/"):
                continue
            try:
                sensor = int(parts[1].strip("/"))
                bpm = float(parts[2])
                stamp = float(parts[3].rstrip("."))
            except ValueError:
                continue
            events.append((stamp / 1000.0, sensor, bpm))
    events.sort()
    start = events[0][0] if events else 0.0
    return [(t - start, s, b) for t, s, b in events]


def read_plan(path: Path) -> Room:
    data = json.loads(path.read_text(encoding="utf-8"))
    room = Room(name=path.stem, columns=int(data["columns"]), rows=int(data["rows"]))
    for key, sensor in data.items():
        if key in ("columns", "rows"):
            continue
        column, row = (int(v) for v in key.split())
        room.ids[(column, row)] = int(sensor)
    return room


def main() -> int:
    parser = argparse.ArgumentParser(description="Rejoue un enregistrement de capteurs VIBH2O vers Unreal.")
    parser.add_argument("record", type=Path, help="Fichier d'enregistrement (RECORDS/*.txt).")
    parser.add_argument("--plan", type=Path, required=True, help="Plan de salle JSON (RoomMapping_Presets).")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9002)
    parser.add_argument("--speed", type=float, default=1.0, help="Vitesse de lecture (2 = deux fois plus vite).")
    parser.add_argument("--start", type=float, default=0.0, help="Commencer a cette seconde de l'enregistrement.")
    parser.add_argument("--duration", type=float, default=0.0, help="Duree a rejouer, en secondes d'enregistrement. 0 = tout.")
    parser.add_argument("--baseline", type=float, default=120.0, help="Secondes du debut qui donnent le BPM de repos.")
    parser.add_argument("--excitation-range", type=float, default=20.0, help="Ecart de BPM qui vaut une excitation de 1.")
    parser.add_argument("--rate", type=float, default=10.0, help="Envois par seconde et par personne.")
    parser.add_argument("--order", choices=("column", "row"), default="column")
    parser.add_argument("--bpm-prefix", default="/BPM")
    parser.add_argument("--excitation-prefix", default="/SD")
    parser.add_argument("--synchrony-prefix", default="/Synchronie")
    parser.add_argument("--mapping-prefix", default="/RoomMapping")
    args = parser.parse_args()

    events = read_record(args.record)
    room = read_plan(args.plan)
    seated = set(room.occupied_ids())
    if not events:
        print("Enregistrement vide ou illisible.")
        return 1
    print(f"Enregistrement : {len(events)} mesures sur {events[-1][0] / 60:.1f} min ; "
          f"plan {room.columns} x {room.rows}, {len(seated)} places.")

    # Resting BPM: the median of each sensor over the first minutes.
    early = defaultdict(list)
    for t, sensor, bpm in events:
        if t > args.baseline:
            break
        if 35.0 < bpm < 200.0:
            early[sensor].append(bpm)
    baseline = {s: statistics.median(v) for s, v in early.items() if v}

    sender = Sender(args.host, args.port, dry_run=False, verbose=False)
    send_room_mapping(sender, room, args.mapping_prefix, args.order, 0.1)
    time.sleep(0.3)

    smooth: dict[int, float] = {}
    trend: dict[int, float] = {}
    index = 0
    while index < len(events) and events[index][0] < args.start:
        index += 1
    clock_start = time.monotonic()
    record_start = args.start
    end = args.start + args.duration if args.duration > 0 else math.inf
    next_send = 0.0
    next_print = 0.0
    try:
        while index < len(events):
            now = record_start + (time.monotonic() - clock_start) * args.speed
            if now > end:
                break
            while index < len(events) and events[index][0] <= now:
                _, sensor, bpm = events[index]
                index += 1
                if sensor in seated and 35.0 < bpm < 200.0:
                    previous = smooth.get(sensor, bpm)
                    smooth[sensor] = previous + (bpm - previous) * 0.2
                    trend[sensor] = smooth[sensor] - previous
            if now >= next_send:
                next_send = now + args.speed / args.rate
                room_trend = statistics.mean(trend.values()) if trend else 0.0
                excitations = []
                for sensor, value in smooth.items():
                    rest = baseline.get(sensor, value)
                    excitation = min(max((value - rest) / args.excitation_range, 0.0), 1.0)
                    excitations.append(excitation)
                    same_way = 1.0 - min(abs(trend.get(sensor, 0.0) - room_trend) / 2.0, 1.0)
                    sender.send(f"{args.bpm_prefix}/{sensor}/", float(value))
                    sender.send(f"{args.excitation_prefix}/{sensor}/", float(excitation))
                    sender.send(f"{args.synchrony_prefix}/{sensor}/", float(same_way))
                if now >= next_print and excitations:
                    next_print = now + 10.0
                    print(f"  {int(now // 60):3d}:{int(now % 60):02d}  {len(smooth):3d} capteurs  "
                          f"excitation moyenne {statistics.mean(excitations):.2f}  max {max(excitations):.2f}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        pass
    finally:
        sender.close()
    print("Fin du rejeu.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
