# CLAUDE.md — contexte projet VIBH2O

> Fichier lu automatiquement par Claude Code au démarrage d'une session dans ce dépôt.
> **Copier ce fichier à la racine du dépôt `VibH2o`** et le compléter au fil du travail.

## Le projet

VIBH2O capte les données cardiaques des spectateurs d'une salle de spectacle en temps réel,
les envoie en **OSC** à un patch **Max/MSP** qui pilote le spectacle, et les restitue aux
spectateurs sur un **dashboard web** accessible par QR code depuis leur siège.

## Architecture (4 dépôts)

```
VIBH2O/
├── VibH2o/            (PRIVÉ) cœur du projet — Max/MSP + scripts
├── VibH2OServer/      serveur Node/Express — dashboard + QR codes
├── VIBH2O_UE/         documentation et passation (ce dépôt)
└── VIBH2O_REBORN/     réécriture Unreal Engine — vide à ce jour
```

### `VibH2o` — cœur du projet

```
Vib-e.motion/
├── VIB.e-motion.maxpat              patch principal
├── VIBH2O_AcquisitionIntervalles.maxpat   acquisition intervalles cardiaques
├── VIBH2O_Mapping.maxpat            mapping données → sorties
├── VibH2O_RoomMapping.maxpat        plan de salle / sièges
├── VIBH2O_TriggerEvents.maxpat      déclenchement d'événements
├── VIBH2O_DetectionEvent.maxpat     détection d'événements
├── EventCreator.maxpat              création d'événements
├── Scripts/         scripts js/jsui appelés PAR les patchs (HRV.js, SensorFilter.js,
│                    FillSeats.js, PresetManager.js, RecordSensor.js, DataMapping.js…)
│                    + OSC_SIMULATOR.py (simule 176 capteurs BPM en OSC)
├── Externals/       CNMAT-Externals 6, Heartbeat
├── Maxpat/          abstractions et médias
├── RoomMapping_Presets/, Sequences/, Dict/, shell/
└── *.json           configurations de spectacle (CONFIGURATION_ALES_1110.json,
                     Config_Ales_FINAL.json, CIRCLE.json, DictSeats.json…)

Ressources/
├── RECORDS/         enregistrements de spectacles (⚠ données personnelles)
├── PyScripts/       analyse de données (MergeFiles, FilterData, CleanBaseline,
│                    ExtractTimestamps, CompareSensorsBetweenFiles, Renumber)
├── Makeseat/        génération des plans de sièges (makeseats.py)
└── MakeDMG/         packaging macOS
```

### `VibH2OServer` — serveur web

```
server.js            Express, port 3000
templates/           dashboard.html, authentication.html, error.html, shared-styles.css
lib/                 chart.min.js, chartjs-plugin-annotation.min.js (versionnés)
scripts/             generate_qr.sh, remove_lines.py, extract_landmarks.py
data/                (ignoré par git) bpm_data.txt, landmarks.txt
```

## Commandes utiles

```shell
# Serveur
cd VibH2OServer && npm install && node server.js     # → http://localhost:3000
                                                      # → http://localhost:3000/user/user1

# QR codes des sièges (macOS, nécessite qrencode)
./scripts/generate_qr.sh 200 qr_output

# Nettoyer les lignes de synchronie d'un enregistrement
python3 scripts/remove_lines.py       # éditer input_file dans le script au préalable

# Simuler 176 capteurs OSC
python3 VibH2o/Vib-e.motion/Scripts/OSC_SIMULATOR.py --host 192.168.1.255 --port 9001
```

## Conventions

- **Branches** : `feat/…`, `fix/…`, `docs/…` — jamais de commit direct sur la branche par défaut.
- **Ne jamais commiter** : `.DS_Store`, `node_modules/`, `data/`, `user/`, `qr_output/`,
  et **aucun enregistrement de spectateur**.
- **RGPD** : les données cardiaques par siège sont des données personnelles. Pas de dépôt
  public, pas d'envoi à un service tiers sans base légale.
- Les scripts de `Vib-e.motion/Scripts/` sont des scripts **Max** (`js`/`jsui`) : ils
  s'exécutent dans les patchs, pas depuis un terminal.

## Points de vigilance

- Le dépôt `VibH2o` n'a **qu'un seul commit** : aucun historique à consulter.
- Les patchs `.maxpat` sont du JSON volumineux (`VIB.e-motion.maxpat` ≈ 940 Ko) :
  les diffs sont illisibles et les modifications se font **dans Max**, pas à la main.
- `generate_qr.sh` suppose l'interface Wi-Fi macOS `en0`.

## Documentation

Guide complet d'installation et de passation : [`PASSATION_CLAUDE_CODE.md`](PASSATION_CLAUDE_CODE.md)
dans le dépôt `VIBH2O_UE`.
