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

## État — 5 septembre 2026

Les étapes 0 à 10 sont **écrites et compilées**. Ce qui a été vérifié pour de bon, et par quel
moyen :

| # | Étape | État | Comment |
|---|---|---|---|
| 0 | Squelette | ✅ | Compile et se lie sous **UE 5.5 et UE 5.8**, cibles Editor, Game et Shipping, sans avertissement. Les 14 tests passent sous les deux moteurs. |
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

### Rejouer les vérifications

```bash
python Tools/selftest_osc.py
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UEEditor Win64 Development -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" VIBH2O_UE.uproject -ExecCmds="Automation RunTests VibH2O;Quit" -unattended -nullrhi -nosplash -NoSound
```

```bash
python Tools/vibh2o_osc_sim.py --preset ales --scenario wave
```

Pour la contrainte « un seul code source pour 5.5 et 5.8 », copier le projet dans un dossier au
chemin **court** — au-dela de 260 caracteres, UnrealBuildTool refuse de compiler — puis pointer le
`Build.bat` de 5.8 dessus. La verification a ete faite ainsi : compilation propre, 14 tests au
vert.

Et la cible qui compte vraiment pour la representation n'est pas l'editeur :

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UE Win64 Shipping -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

La carte, le matériau et la séquence de démonstration se régénèrent à volonté — ce ne sont pas
des assets à préserver :

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" VIBH2O_UE.uproject -run=pythonscript -script="Tools/make_demo_map.py" -unattended -nosplash
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" VIBH2O_UE.uproject -run=pythonscript -script="Tools/make_demo_material.py" -unattended -nosplash
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" VIBH2O_UE.uproject -run=pythonscript -script="Tools/make_demo_sequence.py" -unattended -nosplash
```

### La recette visuelle, en une commande

`VibH2O.DemoSweep` déroule tout seul les critères que les tests numériques ne jugent pas :
courbure dans les deux sens, espacement constant, gradins et relief, focus, capteurs muets,
acteur déplacé, morph vers le vortex et retour, lecture Sequencer, salle de 176 avec mesure
d'images par seconde — **une capture PNG étiquetée par phase** dans `Saved/VibH2OSweep/`, puis
le moteur quitte.

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor.exe" VIBH2O_UE.uproject -game -Windowed -ResX=1280 -ResY=720 -ExecCmds="VibH2O.DemoSweep" -NoSound -unattended -nosplash
```

Une fenêtre s'ouvre environ une minute. Le rendu hors écran (`-RenderOffscreen`) ne convient
**pas** : la capture d'écran attend une présentation d'image qui n'y arrive jamais, et la frame
se fige.

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
| 5 | ~176 sièges : le tout reste fluide | ✅ **517 images/s mesurées** en rendu réel, sweep phase 14 |
| 6 | Acteur de scène déplacé, tourné, redimensionné : tout suit | ✅ capture `08_acteur_deplace` — translation + 35° de yaw, tout suit |
| 7 | Courbure balayée dans les deux sens, droite exacte à zéro | ✅ automatisé au bit près, **et** vu en plongée sur captures |
| 8 | Chaque effet s'active et se désactive isolément | 🟡 bascules testées ; l'isolement visuel effet par effet reste à regarder |
| 9 | Focus : assombrissement et cadrage justes sur salle courbée | ✅ assombrissement sur capture ; bornes testées numériquement |
| 10 | Blend 0 → 1 → 0 sans à-coup, flottement persistant | ✅ captures à 0, ½ et 1, retour capturé ; flottement testé |
| 11 | Trois paramètres animés dans Sequencer | ✅ séquence générée, **lecture en jeu vérifiée** (BlendAlpha 0 → 0,22 par la piste seule) |
| 12 | Capteur coupé : passage en muet en ~5 s, sortie des moyennes | ✅ automatisé |
| 13 | Dans le niveau sous-marin : ombres et caustiques reçues | 🟡 à faire |
| 14 | Depuis le vrai patch Max : adresses confirmées ou réglages ajustés | 🟡 relevées sur les patchs, à confirmer en salle |

Les lignes marquées ✅ se rejouent d'une commande — `VibH2O.DemoSweep` pour les visuelles.
Les deux 🟡 restants demandent ce qu'aucune machine de ce dépôt ne possède : l'environnement
`underwater_bp` de l'artiste (caustiques), et la salle avec le vrai patch Max.

---

## Après cette livraison

- Le **comportement de banc** du tableau 2 : voisinage, séparation, alignement, cohésion.
- La **direction artistique** des bulles : mesh, matériau subsurface, stries, anneaux.
- Un éventuel **pilotage MIDI direct**, à brancher sur les fonctions existantes.
- Les **tableaux suivants**, chacun étant une nouvelle classe de pilote de position.
