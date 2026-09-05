# Feuille de route — construction du plugin

Ordre de construction, avec pour chaque étape un **critère de terminaison vérifiable**. Les
étapes sont ordonnées par dépendance : chacune s'appuie sur la précédente et peut être validée
seule.

> **Rappel :** Unreal n'est pas installé dans un environnement Claude Code distant. Les étapes 1
> et 2 sont vérifiables sans moteur ; à partir de l'étape 3, la validation se fait sur la machine
> Windows de Dimitri. Ne jamais annoncer une compilation qui n'a pas été lancée.

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

| # | Vérification |
|---|---|
| 1 | Tests du parseur au vert |
| 2 | Salle 5 × 5 : 25 bulles, chacune à son tempo |
| 3 | Salle 7 × 3 : **orientation confirmée**, valeur notée dans `OSC_PROTOCOL.md` |
| 4 | Sièges à 0 : aucune bulle, moyennes non faussées |
| 5 | ~176 sièges : le tout reste fluide |
| 6 | Acteur de scène déplacé, tourné, redimensionné : tout suit |
| 7 | Courbure balayée dans les deux sens, droite exacte à zéro |
| 8 | Chaque effet s'active et se désactive isolément |
| 9 | Focus : assombrissement et cadrage justes sur salle courbée |
| 10 | Blend 0 → 1 → 0 sans à-coup, flottement persistant |
| 11 | Trois paramètres animés dans Sequencer |
| 12 | Capteur coupé : passage en muet en ~5 s, sortie des moyennes |
| 13 | Dans le niveau sous-marin : ombres et caustiques reçues |
| 14 | Depuis le vrai patch Max : adresses confirmées ou réglages ajustés |

---

## Après cette livraison

- Le **comportement de banc** du tableau 2 : voisinage, séparation, alignement, cohésion.
- La **direction artistique** des bulles : mesh, matériau subsurface, stries, anneaux.
- Un éventuel **pilotage MIDI direct**, à brancher sur les fonctions existantes.
- Les **tableaux suivants**, chacun étant une nouvelle classe de pilote de position.
