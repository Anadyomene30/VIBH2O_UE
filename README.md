# VIBH2O_UE — plugin Unreal

Plugin **Unreal Engine** qui reçoit en **OSC/UDP** les données cardiaques de spectateurs équipés
de capteurs, et les donne à voir sous forme de **bulles** disposées comme le plan de la salle.

```
   SALLE                    MAC                        PC WINDOWS
 spectateurs   ──────►    Max/MSP      ──OSC/UDP──►      Unreal
  + capteurs             (analyse)                    (ce plugin)
```

> **État : la première livraison est écrite, compilée sous UE 5.5, et couverte par
> 14 tests d'automation au vert.** Ce qui reste tient à ce qu'un test ne remplace pas — un œil, un
> matériau, une vraie salle. Le détail est dans [docs/ROADMAP.md](docs/ROADMAP.md).

## Essayer en trois commandes

```bash
python Tools/selftest_osc.py
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UEEditor Win64 Development -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

```bash
python Tools/vibh2o_osc_sim.py --preset ales --scenario wave
```

**Ou en zéro commande** : ouvrir `VIBH2O_UE.uproject`, carte `Maps/VibH2O_Demo`, **Play** — le
simulateur intégré anime la salle sans Python ni Max, et s'efface de lui-même dès que de vraies
données réseau arrivent. `VibH2O.ShowDebug` dans la console montre l'état si quelque chose cloche.

## Par où commencer

| Document | Contenu |
|---|---|
| **[CLAUDE.md](CLAUDE.md)** | **À lire en premier.** Contexte, contraintes dures, décisions d'architecture avec leur justification, et pièges connus. Lu automatiquement par Claude Code. |
| [Plugins/VibH2O/README.md](Plugins/VibH2O/README.md) | Le mode d'emploi du plugin : installation, classes, contrat matériau, focus, Sequencer. |
| [docs/ROADMAP.md](docs/ROADMAP.md) | L'ordre de construction, et **l'état de chaque étape**. |
| [docs/SPEC.md](docs/SPEC.md) | La liste complète des fonctions à réaliser. |
| [docs/OSC_PROTOCOL.md](docs/OSC_PROTOCOL.md) | Le protocole réseau, sa capture réelle et ses quatre pièges. |

## Ce qui a été construit

```
Plugins/VibH2O/          le plugin, autonome et copiable tel quel
  Source/VibH2O/Public/  parseur OSC, récepteur, réglages, subsystem, acteurs, pilotes
  Source/VibH2O/Private/ implémentations, et 14 tests d'automation
Tools/
  vibh2o_osc_sim.py       simulateur : salles 5×5, 7×3, Alès 23×4, 176 capteurs
  vibh2o_osc_listen.py    écouteur de diagnostic, sans dépendance
  selftest_osc.py         auto-test de l'outillage, sans Unreal
  make_demo_map.py        générateur de la carte de démonstration
  make_demo_material.py   générateur du matériau de démonstration (11 paramètres visibles)
  make_demo_sequence.py   générateur de la séquence Sequencer de recette
```

La recette visuelle se rejoue d'une commande — `VibH2O.DemoSweep` — qui capture chaque critère
en PNG ; voir [docs/ROADMAP.md](docs/ROADMAP.md).

Le plugin **n'utilise pas le plugin OSC d'Epic** : son socket et son parseur OSC 1.0 sont écrits
ici, sans dépendance, pour tenir l'objectif « un seul code source pour 5.5 et 5.8 ».

## L'œuvre en deux tableaux

**Tableau 1 — le plan de salle.** Une bulle par siège occupé, à sa place réelle. Chaque bulle bat
au rythme du cœur de sa personne, grossit, se colore et dérive selon son niveau d'excitation. Un
mode focus permet d'isoler une partie de la salle en assombrissant le reste.

**Tableau 2 — le flock.** Le plan de salle se dissout : les bulles quittent leur siège et forment
un banc enroulé en vortex, un cône qui s'évase vers le haut. La synchronie collective en règle
l'ordre — spirale serrée quand le public synchronise, dispersion quand il décroche. Le passage
d'un tableau à l'autre se conduit à la main, en continu.

*La première livraison couvre le tableau 1 complet, avec l'architecture prête pour le second.*

## Cibles et contraintes

- **Unreal 5.5 aujourd'hui, 5.8 ensuite**, avec un seul code source.
- Le plugin tourne sous **Windows** ; le patch émetteur est sur **Mac**.
- Jusqu'à **~200 bulles** simultanées.
- L'installation est **déplaçable** dans le niveau, et tous ses paramètres sont **animables dans
  Sequencer**.

## Les dépôts du projet

| Dépôt | Contenu |
|---|---|
| [`Anadyomene30/VibH2o`](https://github.com/Anadyomene30/VibH2o) *(privé)* | Cœur du projet : patchs Max/MSP, scripts. Contient `Scripts/OSC_SIMULATOR.py`. |
| [`Anadyomene30/VIBH2O_UE`](https://github.com/Anadyomene30/VIBH2O_UE) | Ce dépôt — le plugin Unreal. |
| [`eva-decorps/VibH2OServer`](https://github.com/eva-decorps/VibH2OServer) | Serveur web : dashboard spectateurs, QR codes par siège. |
| [`Anadyomene30/VIBH2O_REBORN`](https://github.com/Anadyomene30/VIBH2O_REBORN) | Décrit comme la réécriture Unreal, vide à ce jour. |

> L'ancienne documentation de passation de l'écosystème VIBH2O reste accessible dans l'historique
> Git : `git show 7da3dca:PASSATION_CLAUDE_CODE.md`

## Contact

Dimitri Sourzac — `dimitri.sourzac@gmail.com`
