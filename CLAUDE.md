# CLAUDE.md — Plugin Unreal VIBH2O

> Lu automatiquement par Claude Code au démarrage d'une session dans ce dépôt.
> **Lis-le en entier avant d'écrire la moindre ligne de code.**

## Ce qu'il faut savoir en trente secondes

Ce dépôt sert à construire **un plugin Unreal Engine** qui reçoit en **OSC/UDP** les données
cardiaques de spectateurs équipés de capteurs, et les donne à voir sous forme de **bulles**
disposées comme le plan de la salle.

Le plugin **existe** : il compile sous UE 5.5 et ses 14 tests d'automation passent. Ce qui reste
est listé dans [`docs/ROADMAP.md`](docs/ROADMAP.md), et tient à ce qu'un test ne remplace pas — un
œil, un matériau, une vraie salle. Lis, dans l'ordre :

1. Ce fichier — les contraintes et le pourquoi des décisions.
2. [`docs/ROADMAP.md`](docs/ROADMAP.md) — par où commencer, et comment savoir que c'est fait.
3. [`docs/SPEC.md`](docs/SPEC.md) — la liste complète des fonctions à réaliser.
4. [`docs/OSC_PROTOCOL.md`](docs/OSC_PROTOCOL.md) — le protocole réseau et ses pièges.

## Le dispositif

```
   SALLE                    MAC                        PC WINDOWS
 spectateurs   ──────►    Max/MSP      ──OSC/UDP──►      Unreal
  + capteurs             (analyse)                    (ce plugin)
```

Unreal **ne fait que recevoir et afficher**. Toute l'analyse du signal — filtrage, calcul de
variabilité, baseline, synchronie — reste dans Max. Ne propose jamais de déporter du traitement
de signal vers Unreal : ce n'est pas une limite technique, c'est le partage des rôles voulu.

L'œuvre se joue en **tableaux** successifs :

- **Tableau 1 — le plan de salle.** Une bulle par siège occupé, à sa place réelle. Chaque bulle
  bat au rythme du cœur de sa personne, grossit, se colore et dérive selon son excitation.
- **Tableau 2 — le flock.** Le plan se dissout, les bulles forment un banc enroulé en vortex
  (un cône qui s'évase vers le haut). La synchronie collective en règle l'ordre.

**Périmètre de la première livraison : le tableau 1 complet, avec l'architecture prête pour le 2.**

## Contraintes dures

| Contrainte | Détail |
|---|---|
| **Versions** | UE **5.5 aujourd'hui, 5.8 ensuite**, avec **un seul code source**. N'utilise que des API stables entre les deux. |
| **Plateforme** | Le plugin tourne sous **Windows**. Le patch émetteur est sur Mac. |
| **Échelle** | Jusqu'à **~200 bulles** simultanées (le simulateur existant du projet en pousse 176). Un acteur par bulle reste viable à cette échelle ; au-delà il faudrait passer aux meshes instanciés. |
| **Déplaçable** | L'installation est un acteur du niveau : elle doit pouvoir être déplacée, tournée et redimensionnée dans le viewport, tout la suivant. |
| **Sequencer** | Tout paramètre pilotable doit être animable dans Sequencer. |

## Décisions déjà tranchées — et pourquoi

**Ne les remets pas en cause sans raison neuve.** Chacune a été prise en connaissance de cause ;
la raison est donnée pour que tu puisses juger, pas pour que tu la redébattes.

### Réseau : socket UDP et parseur OSC écrits dans le plugin

**Ne pas utiliser le plugin OSC d'Epic.** Son statut et son API bougent d'une version d'Unreal à
l'autre, ce qui rendrait impossible l'objectif « un seul code pour 5.5 et 5.8 ». Un parseur OSC
1.0 fait environ 300 lignes, n'a aucune dépendance moteur, et se teste hors moteur.

### Battement : accumulateur de phase, jamais un timer

Max n'envoie **que le BPM**, pas d'événement par battement. Unreal doit donc fabriquer le rythme.

La méthode est imposée : une **phase** par individu, avançant de `DeltaTime * (BPM / 60)` chaque
frame, qui déclenche un battement en franchissant 1.0 et se décrémente.

Un timer relancé à chaque nouvelle valeur de BPM ferait **sauter** le battement à chaque mise à
jour, ce qui se voit immédiatement. Avec la phase, un changement de BPM ne modifie que la
*vitesse* d'avancement : aucune discontinuité. Cette continuité est aussi ce qui permet aux
anneaux concentriques du matériau de ne jamais glitcher.

Prévois une **entrée d'événement de battement câblée mais inutilisée** : le jour où Max enverra
les battements réels, le branchement suffira.

### Espace local, jamais monde

L'installation étant déplaçable, **toutes les positions se calculent en local** sous la racine de
l'acteur de scène (`RelativeLocation`), y compris l'axe du vortex. Un calcul en coordonnées monde
fonctionnerait tant que rien ne bouge, et casserait au premier déplacement.

### Les bulles n'appartiennent à aucun tableau

C'est **la décision structurante**. Les bulles sont possédées par un acteur de scène permanent ;
un tableau n'est qu'une **fonction qui dit où va chaque bulle**.

Si la salle possédait ses bulles, passer au vortex signifierait les détruire et en créer d'autres
— donc perdre la phase de battement et toute continuité, et rendre impossible le morph progressif
entre les tableaux. Les deux pilotes de position tournent **en permanence** et un curseur
interpole entre eux, ce qui rend la transition continue, réversible et arrêtable à mi-chemin,
sans machine à états. Un tableau 3 sera une classe de plus, pas une réécriture.

### Threading

Le thread réseau n'écrit **que** dans une file (`TQueue` SPSC). Toute la logique objet se fait sur
le game thread, en vidant la file depuis le tick du subsystem. **Tout accès à un `UObject` depuis
le thread réseau est un crash aléatoire en représentation** — c'est-à-dire le pire moment.

### Le plugin ne possède pas l'apparence

Le look est construit par l'artiste dans son Blueprint et son matériau. Le plugin **pousse des
valeurs nommées** dans une instance dynamique de matériau et **émet des événements Blueprint**.
Ne code jamais d'apparence en dur ; fournis une bulle de démonstration fonctionnelle, pas une
direction artistique.

### Le plugin ne pilote pas la caméra

Il **expose** la cible de cadrage (centre, bornes, distance de recul). La caméra reste au
metteur en scène — Cine Camera, Blueprint ou Level Sequence.

## Pièges connus

Ils sont tous décrits en détail dans [`docs/OSC_PROTOCOL.md`](docs/OSC_PROTOCOL.md). En résumé :

1. **Slash final dans les adresses** (`/RoomMapping/columns/`) → ignorer les segments vides.
2. **Le plan de salle arrive en rafale sans marqueur de fin** → attendre ~100 ms de silence avant
   de construire, sinon la salle est reconstruite à chaque message.
3. **Un ID de 0 signifie « siège vide »** → aucune bulle, et exclusion de toutes les moyennes.
4. **L'ordre de parcours des sièges est ambigu** → une transposition est possible entre l'index
   OSC et la lecture visuelle. **Invisible sur une grille carrée.** Rendre l'ordre basculable et
   valider sur une salle non carrée (7 × 3).

Et deux pièges de rendu :

5. **Un matériau translucide ne reçoit correctement ni ombres ni caustiques** dans Unreal. Comme
   l'environnement est sous-marin et que les bulles doivent recevoir les deux, viser un matériau
   **opaque ou masked en subsurface**, la transparence étant feinte par du Fresnel.
6. **Sequencer ne voit une propriété que si elle porte `Interp`** dans son `UPROPERTY`. Sans ce
   spécificateur, elle n'apparaît pas dans les propriétés animables.

## Conventions de travail

- **Le code, les noms de classes et les commentaires en anglais.** La documentation et les
  échanges avec Dimitri en **français**.

  La frontière retenue, en pratique : **ce que lit un développeur** — commentaires, noms — est en
  anglais ; **ce que lit l'opérateur en salle** — messages de log, affichage de débogage, libellés
  d'assertion des tests, noms affichés dans le panneau Details — reste en français. Les catégories
  `UPROPERTY` suivent le panneau Details, donc le français.
- Préfixe des classes : `VibH2O` (`AVibH2OStageActor`, `UVibH2OSubsystem`…).
- Le plugin vit dans `Plugins/VibH2O/` pour pouvoir être copié tel quel dans un autre projet.
- **Ne commite jamais de données de spectacle** — les enregistrements de capteurs sont des
  données personnelles. `.gitignore` couvre `data/` et `RECORDS/`.
- Ne crée pas de pull request sans qu'on te le demande.

## Vérifier son travail

**Ne prétends jamais avoir vérifié un build que tu n'as pas lancé.** C'est la règle, et elle ne
bouge pas. Ce qui change, c'est ce que tu *peux* lancer.

**En environnement Claude Code distant**, le moteur n'est pas installé : écris le code avec une
vigilance particulière sur ce qui casse un build Unreal (macros de réflexion, includes, modules
déclarés dans le `.Build.cs`) et dis clairement que la compilation reste à faire. Deux choses
restent vérifiables sans moteur : le **parseur OSC**, délibérément isolé de tout code moteur, et le
**simulateur Python**, qui n'a besoin que de la bibliothèque standard.

**Sur la machine de Dimitri**, UE 5.5 et 5.8 sont installés sous `C:\Program Files\Epic Games\`.
Tout est donc vérifiable, et doit l'être :

```bash
python Tools/selftest_osc.py
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UEEditor Win64 Development -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" VIBH2O_UE.uproject -ExecCmds="Automation RunTests VibH2O;Quit" -unattended -nullrhi -nosplash -NoSound
```

**Trois cibles, pas une.** L'éditeur ne suffit pas : c'est la cible **Game** qui tourne en
représentation, et elle compile sans l'éditeur. Une API `WITH_EDITOR` — `SetActorLabel`,
`SetFolderPath`, `FPropertyChangedEvent` — passe en éditeur et casse en Game. À garder par
`WITH_EDITOR`, jamais par une macro de configuration comme `!UE_BUILD_SHIPPING`.

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" VIBH2O_UE Win64 Shipping -Project="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -WaitMutex
```

**La contrainte 5.5 / 5.8 se vérifie, elle aussi.** Copier le projet dans un dossier au chemin
**court** — au-delà de 260 caractères UnrealBuildTool refuse de compiler — et lancer le `Build.bat`
de 5.8 dessus. Fait le 5 septembre 2026 : compilation propre, 14 tests au vert sous les deux
moteurs.

> **Piège du build.** `Build.bat` renvoie parfois **0 alors que la compilation a échoué** :
> l'exécuteur réussit, le compilateur non. Lis toujours la sortie, ne te fie pas au code de retour.
> De même, les résultats d'automation ne vont pas sur la sortie standard : ils sont dans
> `Saved/Logs/VIBH2O_UE.log`, sur les lignes `Test Completed. Result=`.

La procédure de recette complète, et l'état de chaque étape, sont dans
[`docs/ROADMAP.md`](docs/ROADMAP.md).

## Ressources du projet

Le projet VIBH2O dépasse ce dépôt :

| Dépôt | Contenu |
|---|---|
| `Anadyomene30/VibH2o` (privé) | Cœur du projet : patchs Max/MSP, scripts. Les chemins réels sont `Vib-e.motion/Scripts/` et `Vib-e.motion/RoomMapping_Presets/`, et non `Scripts/`. |
| `Anadyomene30/VIBH2O_UE` | Ce dépôt — le plugin Unreal. |
| `Anadyomene30/VIBH2O_REBORN` | Décrit comme la réécriture Unreal, vide à ce jour. À clarifier avec Dimitri si la question du dépôt d'accueil se repose. |

Le patch `VibH2O_RoomMapping.maxpat` est celui qui émet le plan de salle : c'est la source de
vérité du protocole si un doute apparaît.

**Trois fichiers de ce dépôt ont déjà tranché des points ouverts** — les relire avant de rouvrir
une question :

- `Vib-e.motion/Scripts/FormatRoomMapping.js` — la formule d'index de siège, donc le piège 4.
- `Vib-e.motion/RoomMapping_Presets/ALES_FINAL.json` — le vrai plan d'Alès, 23 × 4 avec deux
  allées. Non carré **et** troué : le meilleur cas de test qui existe.
- `Vib-e.motion/Scripts/OSC_SIMULATOR.py` — attention, il simule les **capteurs en amont de Max**
  (`/oh1/<hex>/bpm`), pas le flux que le plugin écoute. L'étendre serait un contresens ;
  `Tools/vibh2o_osc_sim.py` en reprend les conventions et émet le bon maillon.

## Contact

Dimitri Sourzac — `dimitri.sourzac@gmail.com`

Les points encore ouverts sont listés en fin de [`docs/SPEC.md`](docs/SPEC.md). Aucun ne bloque le
démarrage : ils portent sur des noms d'adresses OSC et des plages de valeurs, tous rendus
configurables pour que le code n'ait pas à être repris.
