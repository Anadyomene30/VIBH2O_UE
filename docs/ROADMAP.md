# Feuille de route — construction du plugin

Ordre de construction, avec pour chaque étape un **critère de terminaison vérifiable**. Les
étapes sont ordonnées par dépendance : chacune s'appuie sur la précédente et peut être validée
seule.

> **Rappel :** Unreal n'est pas installé dans un environnement Claude Code distant. Les étapes 1
> et 2 sont vérifiables sans moteur ; à partir de l'étape 3, la validation se fait sur la machine
> Windows de Dimitri. Ne jamais annoncer une compilation qui n'a pas été lancée.
>
> **Sur la machine de Dimitri, en revanche, tout est vérifiable** : UE 5.5 y est installé, et les
> commandes ci-dessous ont réellement été exécutées.

---

## État — 22 octobre 2026

Les étapes 0 à 10 sont **écrites et compilées**, et la recette visuelle ne laisse plus que les
deux points qu'aucune machine de ce dépôt ne peut trancher. Ce qui a été vérifié pour de bon, et
par quel moyen :

| # | Étape | État | Comment |
|---|---|---|---|
| 0 | Squelette | ✅ | Compile et se lie sous **UE 5.5 et UE 5.8**, cibles Editor, Game et Shipping, sans avertissement. Les 15 tests passent sous les deux moteurs. |
| 1 | Parseur OSC | ✅ | 5 tests d'automation, dont une trame de référence écrite à la main et 2 000 trames aléatoires. |
| 2 | Simulateur | ✅ | `Tools/selftest_osc.py` au vert, boucle UDP comprise. |
| 3 | Socle réseau et état | ✅ | 2 tests d'intégration + un essai live à 2 500 msg/s. |
| 4 | Salle et bulles | ✅ | **84 bulles créées** depuis le plan réel d'Alès, allées comprises. |
| 5 | Battement et effets | ✅ | Phase, dérive et flottement testés ; anneaux et pop visibles sur les captures du sweep. |
| 6 | Contrat matériau | ✅ | `M_VibH2ODemoBubble` généré par `Tools/make_demo_material.py` — les onze paramètres ont chacun un effet visible distinct, constaté sur captures. |
| 7 | Géométrie de salle | ✅ | Courbure testée au bit près, **et** vue en plongée dans les deux sens sur captures ; gradins et relief vus de profil. |
| 8 | Focus | ✅ | Assombrissement hors groupe constaté sur capture ; bornes testées numériquement. |
| 9 | Tableaux et transition | ✅ | Morph capturé à 0, mi-course et 1 ; le cône se lit de profil, teinté par `BlendAlpha`. La logique de banc reste hors périmètre, comme prévu. |
| 10 | Sequencer et finitions | ✅ | `LS_VibH2O_Recette` (générée par script) anime les trois paramètres ; **lecture vérifiée en jeu** : BlendAlpha passe de 0 à 0,22 sous le seul contrôle de la piste. |

### Ce qui s'est ajouté depuis les étapes 0 à 10

Trois apports qui ne figuraient pas dans le découpage initial, chacun compilé et mesuré :

| Apport | État | Comment |
|---|---|---|
| **Simulateur intégré** (`AVibH2OSimulatorActor`) | ✅ | Joue le rôle de Max depuis le niveau. Play sur la carte de démo suffit — sans Python, sans terminal. |
| **Prévisualisation animée en éditeur** | ✅ | `VibH2O.PreviewProbe` mesure `StageTime` sur trois secondes de tick éditeur : 0,000 → 1,311, hors Play. La documentation l'affirmait, la sonde le prouve. |
| **Passe artistique** | ✅ | Silhouette organique, rampe de couleur à trois paliers, **vrais boids** pour le tableau 2, numéros de siège, particules, éclairage volumétrique. |
| **Isolement des effets** (`VibH2O.EffectIsolation`) | ✅ | Le point 8 de la recette, devenu mesurable. Voir ci-dessous. |
| **Cues MIDI / OSC / clavier** (`AVibH2OCueDirector`) | ✅ sauf Resolume réel | Compilé en 5.5 **et** 5.8, Editor et Shipping ; 19 tests au vert sous les deux moteurs, dont 4 pour les cues. Essai en jeu le 24 octobre, **Resolume Arena au premier plan** : note MIDI, OSC de colonne, `/Cue/<nom>` et Ctrl+Maj+F12 ont lancé, mis en pause, fait sauter et arrêté une Level Sequence. Reste à brancher la sortie OSC de Resolume elle-même. Voir [`CUES.md`](CUES.md) et [`DECISIONS_NUIT.md`](DECISIONS_NUIT.md). |

### Passe du 22 octobre 2026 — réalisme, lumière, éditeur — **compilée et testée, reste à juger à l'œil**

Écrite en session distante, sans moteur, puis vérifiée le même jour sur la machine :

| Demande | Réponse |
|---|---|
| Déformation trop forte, pas réaliste | `OrganicDeform` 0,16 → **0,035**, `BeatPopAmount` 0,18 → **0,10**, part de l'excitation réduite. |
| Bulles sensibles à la lumière | `M_VibH2ODemoBubble` passe d'émissif à **Subsurface** opaque : simulation de transparence par Fresnel, diffusion en contre-jour, reflet mouillé. Battement gardé en pulsation interne discrète. |
| De belles lumières | Soleil remis **vers le bas**. Il était tourné vers le haut : `unreal.Rotator` prend (roll, pitch, yaw). Caustiques en *light function*, fond marin, capture de reflets, Lumen, rig de trois projecteurs. |
| L'éclairage doit s'adapter au nombre de bulles | `AVibH2OLightRig` : cadrage, recul et puissance recalculés à chaque image depuis les bulles affichées, lissés. |
| Voir sans être en Play | Doublure de subsystem propre au monde éditeur ; simulateur et scène y tournent, avec les **vraies bulles**. |

Vérifié le 22 octobre 2026 :

| Vérification | Résultat |
|---|---|
| Compilation UE 5.5, Editor **et** Shipping | ✅ propre, sans avertissement |
| Compilation UE 5.8, Editor (copie au chemin court) | ✅ propre |
| Compilation UE 5.8, **Game et Shipping** (`RunUAT BuildPlugin`) | ✅ après deux includes manquants — `FRichCurve`, `UStaticMesh` — que le PCH de l'éditeur masquait. Vérifié le 22 octobre, le soir. |
| Salle vivante dans l'éditeur, carte lourde sous 5.8 | ✅ après correctif : le plan de salle pouvait s'établir sans que la scène reçoive `OnRoomChanged`, l'éditeur restait vide. La scène rattrape désormais la génération du plan. |
| Tests d'automation | ✅ **15 / 15** sous 5.5 **et** sous 5.8, dont `VibH2O.Integration.PrevisualisationEditeur` |
| `VibH2O.EffectIsolation` | ✅ six effets, six CONFORME — la déformation réduite reste mesurable |
| Matériaux, carte et séquence régénérés | ✅ après correction de `make_demo_map.py` (voir ci-dessous) |
| Images par seconde, salle de 176 | ✅ **303 images/s** (517 avant Lumen et les projecteurs) — large marge |
| Captures du sweep | ✅ bulles éclairées par la scène, caustiques et ombres visibles sur le fond |

`make_demo_map.py` ne passait pas : il vérifiait les nouveaux matériaux par `does_asset_exist`,
qui, dans un commandlet, ne voit pas un asset de plugin écrit juste avant — et il le faisait
**après** avoir supprimé l'ancienne carte, laissant une carte à moitié construite. Le contrôle
passe en tête du script, par `load_asset`.

Reste à faire, à l'œil :

1. Ouvrir la carte, viewport en temps réel (`Ctrl + R`) : la salle doit vivre sans Play.
2. Juger, puis régler : exposition (`PostProcess`), niveaux du rig, taille des caustiques
   (`ridge_of` dans `make_demo_material.py`). Sur les captures, les bulles se lisent plutôt comme
   des **perles** que comme des bulles : la transparence feinte par Fresnel reste discrète.

### Passe du 23 octobre 2026 — trois états de salle, bulle à stries — **compilée, testée, capturée**

Demande de Dimitri : trois états lisibles (calme, normal, excité) selon l'écart collectif à la
baseline, distincts de la synchronie ; le look des bulles de `VIBH2o_PPCM`, en mieux ; des couleurs
réglables dans l'éditeur ; une interface rapide sur place. Détail dans le
[README du plugin](../Plugins/VibH2O/README.md), section *L'ambiance de la salle*.

| Demande | Réponse |
|---|---|
| Trois états | `FVibH2OMoodTracker` : moyenne lissée (1,5 s), hystérésis (±0,04), fondu de 4 s. Seuils réglables, forçage manuel, état tenu sans capteur actif. |
| Ce qui change | Couleur (palette par état), énergie (lumière, vitesse et agitation des stries), mouvement (flottement, dérive), taille et battement. Les vitesses passent par des **accumulateurs de phase** : un changement d'état ne fait rien sauter. |
| Synchronie à part | Elle règle la **cohérence** : stries alignées sur la salle, teintes resserrées, halo. Jamais la palette. |
| Look PPCM en mieux | Relevé en lecture seule dans le projet LUX : `Sphere_BP` → `Bubble_Mat` (Masked, tramage temporel, tout émissif, stries `sin` sur V). Refait en **Subsurface masked deux faces** : éclairé, ombres et caustiques reçues, stries pilotées par l'état, anneau de lumière à chaque battement. |
| Couleurs dans l'éditeur | Catégorie `VibH2O|Ambiance` en tête ; palettes animables ; presets `UVibH2OMoodPreset` (deux livrés, `Tools/make_mood_presets.py`), bouton d'enregistrement. |

Vérifié le 23 octobre 2026, dans la branche `claude/bulles-look-etats-6977f1` :

| Vérification | Résultat |
|---|---|
| UE 5.5 Editor et Shipping | ✅ propre, sans avertissement |
| Tests d'automation | ✅ **19 / 19** — les 15 d'avant et 4 nouveaux `VibH2O.Ambiance.*` (trois états, pas de clignotement sur une minute de bruit à cheval sur le seuil, fondu de 4 s sans saut, palettes) |
| `VibH2O.EffectIsolation` | ✅ six effets, six CONFORME — après correction : couper la teinte coupe désormais **aussi** la couleur d'état |
| Les trois états au simulateur | ✅ `VibH2O.DemoSweep` phases 15 à 22, par le vrai chemin de données : calme à 0,09, normal à 0,40, excité à 0,85, puis chaque état synchrone et désynchrone. `VibH2O.DemoSweep ambiance` rejoue ces seules phases en une minute. |
| Images par seconde, salle de 176 | 🟡 **182 images/s**, contre 303 avant — mesure faite pendant qu'un éditeur 5.8 d'une autre session occupait le même GPU. Large marge, mais à remesurer seul. |
| UE 5.8, Editor et Shipping (copie au chemin court `C:\VB58bulles`) | ✅ propre, zéro avertissement ; **19 / 19** tests |

Captures avant / après : `Saved/VibH2OSweep_ambiance_2026-10-23/PLANCHE_avant_apres.png` à la
racine du dépôt principal (hors git). Les choix faits sans Dimitri, à valider, sont dans
[`DECISIONS_NUIT.md`](DECISIONS_NUIT.md). Non fait : l'essai dans une copie de PPCM, l'éditeur LUX
étant occupé et son plugin partagé entre branches (décision 10).

Trois pièges rencontrés, notés pour la suite :

1. **Exposition manuelle +10 EV** dans la carte de démo (+10,5 dans PPCM) : un émissif de 1 y est
   mille fois blanc. Les niveaux du matériau sont donc minuscules (`StripeLight` 0,05). Le nœud
   `EyeAdaptationInverse`, essayé pour rendre l'émissif indépendant de l'exposition, n'a rien changé
   de mesurable sous exposition manuelle. Pour une autre scène : `Lumiere des bulles` sur l'acteur.
2. **Nœud `Sine`** : il a déjà une période de 1, il calcule sin(2π·x). Multiplier encore par 2π
   donne quatre cents bandes par bulle, qui se fondent en une lueur uniforme.
3. **Broche d'exposant du nœud `Power`** : refusée sous le nom `Exponent`, et le repli sur l'entrée
   sans nom branchait… la base. Les stries valaient 9 partout. Le générateur n'a plus de repli
   silencieux pour cette broche.

Et un problème hors périmètre : dans ce worktree, `make_demo_material.py` plante **après** avoir
écrit la bulle et les particules, à la recréation des caustiques (assertion Slate dans
`AssetTools.CreateAsset`). Bulle et particules sont bien écrites ; seule la bulle est commitée.

### Attracteurs, zones d'exclusion, télécommande iPad — 23 octobre 2026

Demandés pour piloter la salle à la main pendant le spectacle. Mode d'emploi complet :
[`ATTRACTEURS_ET_IPAD.md`](ATTRACTEURS_ET_IPAD.md).

| Vérification | Résultat |
|---|---|
| Compilation UE 5.5, Editor **et** Shipping | ✅ propre, sans avertissement |
| Compilation UE 5.8, Editor, Game **et** Shipping (`RunUAT BuildPlugin`, copie au chemin court) | ✅ propre, sans avertissement ; la page de la télécommande part bien avec le plugin |
| Tests d'automation | ✅ **19 / 19** sous 5.5, dont les quatre nouveaux : `Steering.FonduActivation`, `Steering.PartDeLaSalle`, `Steering.ProjectionHorsZone`, `Remote.Telecommande` |
| De bout en bout, jeu lancé sur la carte de démo | ✅ `Tools/vibh2o_remote_check.py` : page, état, appel d'un attracteur (mi-chemin à 0,8 s, ni recul ni saut), contre-ordre, retour exact à 0, zone déplacée, éteinte, rallumée |
| Interface vue dans un navigateur au format iPad | ✅ appui sur un attracteur et glisser d'une zone reçus par Unreal |

Reste à faire, sur place :

1. Ouvrir la page **sur le vrai iPad**, l'ajouter à l'écran d'accueil, la juger dans le noir.
2. Placer les zones devant les deux circassiennes réelles, la *Camera de projection* désignée, et
   vérifier à l'image que les bulles s'écartent bien.

### Rejouer les vérifications

```bash
python Tools/selftest_osc.py
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UEEditor Win64 Development -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -ExecCmds="Automation RunTests VibH2O;Quit" -unattended -nullrhi -nosplash -NoSound -abslog="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/Saved/Logs/AutomationRun.log"
```

```bash
python Tools/vibh2o_osc_sim.py --preset ales --scenario wave
```

Pour la contrainte « un seul code source pour 5.5 et 5.8 », copier le projet dans un dossier au
chemin **court** — au-dela de 260 caracteres, UnrealBuildTool refuse de compiler — puis pointer le
`Build.bat` de 5.8 dessus. La verification a ete faite ainsi : compilation propre, 15 tests au
vert. Elle ne couvrait que l'editeur : la cible Game de 5.8 se verifie a part, par
`RunUAT.bat BuildPlugin -Plugin=<uplugin> -Package=<dossier court> -TargetPlatforms=Win64 -Rocket`,
qui compile Editor, Game et Shipping d'un coup.

Et la cible qui compte vraiment pour la representation n'est pas l'editeur :

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UE Win64 Shipping -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

La carte, le matériau et la séquence de démonstration se régénèrent à volonté — ce ne sont pas
des assets à préserver. **Dans cet ordre** — la carte a besoin des matériaux, la séquence de la
carte :

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -run=pythonscript -script="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/Tools/make_demo_material.py" -unattended -nosplash
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -run=pythonscript -script="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/Tools/make_demo_map.py" -unattended -nosplash
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -run=pythonscript -script="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/Tools/make_demo_sequence.py" -unattended -nosplash
```

> **`-script` aussi en absolu.** Un chemin relatif se résout depuis `Engine/Binaries/Win64` du
> moteur, pas depuis le dépôt : `Could not load Python file`, et rien n'est régénéré. Le commandlet
> le dit dans son log, mais pas sur la sortie standard.

### La recette visuelle, en une commande

`VibH2O.DemoSweep` déroule tout seul les critères que les tests numériques ne jugent pas :
courbure dans les deux sens, espacement constant, gradins et relief, focus, capteurs muets,
acteur déplacé, morph vers le vortex et retour, lecture Sequencer, salle de 176 avec mesure
d'images par seconde — **une capture PNG étiquetée par phase** dans `Saved/VibH2OSweep/`, puis
le moteur quitte.

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -game -Windowed -ResX=1280 -ResY=720 -ExecCmds="VibH2O.DemoSweep" -NoSound -unattended -nosplash
```

Une fenêtre s'ouvre environ une minute. Le rendu hors écran (`-RenderOffscreen`) ne convient
**pas** : la capture d'écran attend une présentation d'image qui n'y arrive jamais, et la frame
se fige.

> **Piège du lancement.** Donner le `.uproject` en chemin **absolu**. Avec un chemin relatif, et
> selon le shell, le moteur écrit `Project file not found`, sort en trois lignes **sans écrire le
> moindre log** — et celui de la veille reste en place, prêt à être relu comme s'il était du jour.
> Vérifier l'horodatage du log avant d'en croire une ligne. `-abslog` n'est pas obligatoire, mais
> donne à chaque lancement son fichier.

### L'isolement des effets, en une commande

`VibH2O.EffectIsolation` répond au point 8 de la recette : non pas « la bascule bascule-t-elle »,
que les tests d'automation couvrent déjà, mais **« couper un effet ne dérange-t-il que ce qu'il
prétend gouverner »**. Elle coupe les six effets un par un et mesure sept canaux à chaque fois —
position, rotation, échelle, silhouette, teinte, focus, battement — puis rend un verdict par
effet, plus une capture PNG par étape dans `Saved/VibH2OIsolation/`, toutes prises depuis la
**même caméra fixe**, donc comparables à l'œil.

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -game -Windowed -ResX=1280 -ResY=720 -ExecCmds="VibH2O.EffectIsolation" -NoSound -unattended -nosplash -abslog="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/Saved/Logs/Isolation.log"
```

Sans les captures, en une trentaine de secondes et sans fenêtre, `-nullrhi` suffit : le verdict
chiffré est le même.

**Comment elle évite de se mentir**, car c'est là qu'était toute la difficulté :

1. **Le simulateur intégré est arrêté.** Il tourne dans la carte de démo et injecterait ses
   propres valeurs par-dessus celles de la mesure. Première version sans cet arrêt : bruit de
   fond de 27 unités sur la position, tous les verdicts noyés.
2. **Les horloges sont mises à zéro** — flottement, rotation de la dérive, déformation organique.
   Aucune de leurs périodes ne divise une fenêtre de mesure, si bien que leur seul défilement
   passait pour du mouvement. À vitesse nulle, chaque contribution garde l'offset que lui donne sa
   graine : toujours non nulle, donc toujours mesurable quand on la coupe.
3. **Une référence est intercalée entre chaque effet**, et chaque effet est comparé au milieu des
   deux références qui l'encadrent — ce qui annule toute dérive lente.
4. **L'instrument se mesure lui-même** : chaque référence intérieure est jugée contre ses deux
   voisines par la formule exacte des effets. Le pire chiffre obtenu est le bruit de fond, et un
   canal n'a bougé que s'il le dépasse au triple.

Résultat du 9 septembre 2026, en rendu réel comme en `-nullrhi` : **six effets, six verdicts
CONFORME**. Marge confortable — la position bouge de 14 à 26 unités quand on coupe le flottement
ou la dérive, pour un bruit de fond de 0,04.

Et parce qu'un harnais toujours vert ne prouve rien, il a été **saboté exprès** : en déclarant
que la teinte devait déplacer les bulles, le verdict est passé à `ECART SANS EFFET sur position;
FUITE sur teinte;`. Il sait donc échouer, dans les deux sens.

---

## Étape 0 — Mettre en place le squelette

Créer la structure du projet et du plugin.

```
VIBH2O_UE.uproject
Config/DefaultEngine.ini, DefaultGame.ini
Plugins/VibH2O/
  VibH2O.uplugin                     sans EngineVersion figée → charge sur tout 5.x
  Source/VibH2O/VibH2O.Build.cs      Core, CoreUObject, Engine, Sockets, Networking,
                                     DeveloperSettings
  Source/VibH2O/Public/  Private/
Tools/
```

**Terminé quand** le projet s'ouvre dans Unreal 5.5 et que le plugin apparaît, activé, dans la
liste des plugins.

---

## Étape 1 — Le parseur OSC

**C'est ici qu'il faut commencer réellement.** Le parseur n'a aucune dépendance au moteur, donc
il est le seul morceau entièrement testable sans Unreal — et tout le reste repose dessus.

- `FVibH2OOscMessage` : une adresse et des arguments typés.
- `FVibH2OOscParser` : décodage des messages et des bundles, types `i` / `f` / `s`, padding à
  4 octets.
- Tolérance au **slash final** et aux segments vides.
- Aucun crash sur trame tronquée ou mal formée.

**Terminé quand** les tests d'automation passent sur des trames d'octets connues, couvrant :
adresse à slash final, chacun des trois types, un bundle, et une trame tronquée.

```shell
UnrealEditor-Cmd.exe <projet>.uproject -ExecCmds="Automation RunTests VibH2O" -unattended -nullrhi
```

---

## Étape 2 — Le simulateur OSC

À faire tôt : sans lui, rien de la suite ne se teste.

**Partir de `Scripts/OSC_SIMULATOR.py` du dépôt `VibH2o`** (privé), qui simule déjà 176 capteurs
— l'étendre plutôt que de repartir de zéro.

Cas à couvrir : salle 5 × 5, salle **7 × 3** (le test de transposition), sièges à 0, ~176 sièges,
vague d'excitation, coupure d'un individu, balayage de synchronie.

**Terminé quand** le simulateur émet un plan de salle complet et un flux de données, et qu'un
outil d'écoute OSC tiers voit passer les messages attendus.

---

## Étape 3 — Le socle réseau et l'état

- `FVibH2OOscReceiver` : `FRunnable` + `FSocket`, écrivant dans une `TQueue` SPSC.
- `UVibH2OSubsystem` : `UGameInstanceSubsystem` + `FTickableGameObject`, vidant la file sur le
  game thread.
- `UVibH2OSettings` : `UDeveloperSettings` — port, adresse d'écoute, préfixes OSC, délai de
  silence, délai de capteur muet.
- Le modèle de salle, l'état par individu, la synchronie collective.

**Rappel : aucun accès à un `UObject` depuis le thread réseau.**

**Terminé quand**, simulateur lancé, un affichage de débogage montre les bonnes dimensions de
salle, le bon nombre d'individus, et des valeurs qui évoluent — sans qu'aucune bulle n'existe
encore.

---

## Étape 4 — La salle et les bulles

- `AVibH2OStageActor` : racine de transform, propriétaire des bulles.
- `AVibH2OBubbleActor` : classe de base dérivable en Blueprint.
- Placement en grille, en **espace local**.
- Reconstruction **par différence**, et gestion des sièges vides.

**Terminé quand** la salle apparaît avec une bulle par siège occupé, portant le bon identifiant ;
qu'un nouveau plan met la salle à jour sans tout faire clignoter ; et que **déplacer, tourner et
redimensionner l'acteur de scène emmène toute la salle avec lui**.

### ⚠️ Le test qui compte

Lancer le simulateur en **7 colonnes × 3 rangées**. Si l'orientation est fausse, basculer
`SeatOrder` et **noter la bonne valeur dans `OSC_PROTOCOL.md`**.

Ce test ne peut pas être remplacé par une salle carrée, où une transposition est invisible.

---

## Étape 5 — Le battement et les effets

- Accumulateur de phase par individu, dérivant le battement du BPM.
- Pop, taille, couleur, dérive bornée, flottement permanent.
- Mesure de la vitesse de chaque bulle.
- Bascules d'effets, à portée globale.

**Terminé quand** chaque bulle bat à son propre tempo, que les effets s'activent et se désactivent
indépendamment, qu'aucune bulle ne sort de sa case, et que les bulles flottent avec des phases
décalées et non à l'unisson.

**Vérification critique :** faire varier brutalement le BPM et confirmer que `BeatPhase` progresse
**sans discontinuité**. C'est ce qui garantit que les anneaux du matériau ne sauteront pas.

---

## Étape 6 — Le contrat matériau

- Instance dynamique créée sur le mesh désigné par le Blueprint.
- Les neuf paramètres poussés à chaque frame.
- Les événements Blueprint `OnBeat` et `OnDataUpdated`.
- Une bulle de démonstration, fonctionnelle et sans prétention artistique.

**Terminé quand** un matériau de test réagit visiblement à chacun des neuf paramètres.

---

## Étape 7 — La géométrie de salle

Courbure, éventail, orientation, gradins, variation de profondeur.

**Terminé quand** un balayage de `CurvatureAngle` du négatif au positif courbe la salle dans les
deux sens et la ramène **exactement** droite à zéro, que les bulles pivotent bien, et que la
variation de profondeur donne du relief **sans rendre les rangées illisibles**.

---

## Étape 8 — Le focus

Groupes nommés, assombrissement progressif, exposition de la cible de cadrage.

**Terminé quand** un groupe « haut à gauche » assombrit progressivement le reste de la salle, et
que les fonctions de cadrage renvoient des valeurs justes **sur une salle courbée et après
déplacement de l'acteur de scène** — les deux cas où une implémentation naïve échoue.

---

## Étape 9 — Tableaux et transition

- L'abstraction de pilote de position.
- Le pilote de grille et l'amorce de vortex.
- Le mélange continu entre les deux.

**Terminé quand** pousser `BlendAlpha` de 0 à 1 dissout la salle en vortex sans à-coup, que le
retour en arrière est propre, et que **le flottement persiste à `BlendAlpha = 1`**.

---

## Étape 10 — Sequencer et finitions

- `Interp` sur tous les paramètres pilotables.
- L'arbitrage OSC / Sequencer.
- Le README d'installation et le contrat matériau.

**Terminé quand** `BlendAlpha`, `FocusAmount` et `CurvatureAngle` apparaissent comme propriétés
animables, s'animent au scrub, et qu'un message OSC entrant ne perturbe pas une lecture lorsque
`bIgnoreOscControl` est actif.

---

## Recette finale

Avant de considérer la première livraison terminée :

| # | Vérification | État |
|---|---|---|
| 1 | Tests du parseur au vert | ✅ automatisé |
| 2 | Salle 5 × 5 : 25 bulles, chacune à son tempo | ✅ anneaux par bulle sur captures, à son BPM propre (fréquence des anneaux) |
| 3 | Salle 7 × 3 : **orientation confirmée**, valeur notée dans `OSC_PROTOCOL.md` | ✅ notée, et vérifiée sur le plan d'Alès |
| 4 | Sièges à 0 : aucune bulle, moyennes non faussées | ✅ automatisé |
| 5 | ~176 sièges : le tout reste fluide | ✅ **303 images/s mesurées** en rendu réel avec Lumen et le rig de projecteurs (517 avant), sweep phase 14 |
| 6 | Acteur de scène déplacé, tourné, redimensionné : tout suit | ✅ capture `08_acteur_deplace` — translation + 35° de yaw, tout suit |
| 7 | Courbure balayée dans les deux sens, droite exacte à zéro | ✅ automatisé au bit près, **et** vu en plongée sur captures |
| 8 | Chaque effet s'active et se désactive isolément | ✅ `VibH2O.EffectIsolation` — **6 effets, 6 verdicts CONFORME**, sept canaux mesurés, plus une capture par effet depuis une caméra fixe |
| 9 | Focus : assombrissement et cadrage justes sur salle courbée | ✅ assombrissement sur capture ; bornes testées numériquement |
| 10 | Blend 0 → 1 → 0 sans à-coup, flottement persistant | ✅ captures à 0, ½ et 1, retour capturé ; flottement testé |
| 11 | Trois paramètres animés dans Sequencer | ✅ séquence générée, **lecture en jeu vérifiée** (BlendAlpha 0 → 0,22 par la piste seule) |
| 12 | Capteur coupé : passage en muet en ~5 s, sortie des moyennes | ✅ automatisé |
| 13 | Dans le niveau sous-marin : ombres et caustiques reçues | 🟡 à faire |
| 14 | Depuis le vrai patch Max : adresses confirmées ou réglages ajustés | 🟡 relevées sur les patchs, à confirmer en salle |

Les lignes marquées ✅ se rejouent d'une commande — `VibH2O.DemoSweep` pour les visuelles,
`VibH2O.EffectIsolation` pour l'isolement des effets. Les deux 🟡 restants demandent ce
qu'aucune machine de ce dépôt ne possède : l'environnement `underwater_bp` de l'artiste
(caustiques), et la salle avec le vrai patch Max. **Ce sont les deux seuls points ouverts de la
première livraison.**

---

## Après cette livraison

- ~~Le **comportement de banc** du tableau 2~~ — fait : vrais boids (séparation, alignement, cohésion, attracteur mouvant), pilote par défaut du tableau 2, la synchronie collective serrant ou dispersant le banc. Reste à le juger en salle.
- La **direction artistique** des bulles : avancée le 23 octobre — bulle à stries façon PPCM, en
  subsurface masked, et trois états de salle (voir plus haut). Reste à la juger dans `VIBH2o_PPCM`,
  sur le tulle, et à la régler en salle.
- Un éventuel **pilotage MIDI direct**, à brancher sur les fonctions existantes.
- Les **tableaux suivants**, chacun étant une nouvelle classe de pilote de position.
