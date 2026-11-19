# Cues — piloter Unreal depuis Resolume, sans jamais cliquer dans Unreal

Pendant la représentation, l'opérateur reste dans **Resolume**. Unreal tourne derrière, envoie son
image en Spout, et **suit** : une colonne lancée dans Resolume, un pad frappé sur le contrôleur,
et Unreal lance, arrête ou saute dans une Level Sequence, règle `BlendAlpha`, active un
attracteur.

Ce document dit ce qui marche sans le focus et pourquoi, comment configurer Resolume et loopMIDI,
et comment remplir la liste des cues.

---

## 1. Ce qui arrive à Unreal quand il n'a pas le focus

| Entrée | Sans le focus ? | Pourquoi |
|---|---|---|
| **OSC** | ✅ toujours | C'est du réseau : le socket reçoit, que la fenêtre ait le focus ou non. |
| **MIDI** | ✅ toujours | Une entrée MIDI est ouverte au niveau du système (WinMM), pas d'une fenêtre. |
| **Clavier, entrée normale d'Unreal** | ❌ | Windows n'envoie les touches qu'à la fenêtre qui a le focus. Les *Action Mappings* du projet LUX (`Seq0`, `Seq1` sur le pavé numérique…) ne marchent donc que si l'on a cliqué dans Unreal. |
| **Clavier, lecture « en arrière-plan » du plugin** | ✅ si activée | Voir § 6. Désactivée par défaut. |

**Vérifié le 24 octobre 2026**, Resolume Arena au premier plan (titre de la fenêtre active relevé à
chaque envoi par `Tools/vibh2o_cue_check.py`) : une note MIDI, un message OSC de colonne,
`/Cue/Vortex` et Ctrl+Maj+F12 ont chacun déclenché leur cue, et la séquence a avancé en temps
réel derrière Resolume (10,00 s → 12,05 s en 2,05 s).

### Le ralentissement en arrière-plan

Les cues arrivent, mais encore faut-il que le rendu ne ralentisse pas quand Unreal est derrière
Resolume, sinon le Spout saccade. Trois mécanismes peuvent le freiner ; le plugin s'occupe des
trois dès que **Pleine vitesse en arrière-plan** est coché (Project Settings > Plugins > VibH2O >
Cues — coché par défaut) :

| Mécanisme | Où | Ce que fait le plugin |
|---|---|---|
| `t.IdleWhenNotForeground` | jeu | Le remet à 0 s'il a été monté (il vaut 0 par défaut). |
| *Use Less CPU when in Background* | éditeur, donc Play In Editor | Lève le bridage CPU pendant Play, par le délégué prévu à cet effet par l'éditeur — y compris quand toutes les fenêtres sont réduites. **Mais** ce réglage coupe aussi le rafraîchissement des viewports de l'éditeur en arrière-plan, et cela seul le décocher l'évite. Le plugin l'écrit dans le log s'il est coché. Dans le projet LUX il est **déjà décoché** (`Config/DefaultEditorSettings.ini`). |
| Bridage d'énergie de Windows 11 (EcoQoS) | système | Le processus s'en retire, comme Windows le documente (`SetProcessInformation`). |

L'état est affiché par `VibH2O.ShowDebug`, ligne « cues » : focus, `t.IdleWhenNotForeground`,
réglage de l'éditeur.

> **Ne jamais réduire la fenêtre Unreal** (bouton « _ »). Une fenêtre réduite n'est plus rendue,
> quels que soient ces réglages. La laisser derrière Resolume, ou sur un autre écran.

---

## 2. Trois façons de relier Resolume à Unreal

Unreal et Resolume tournent sur **le même PC** (le Spout l'exige). Par ordre de préférence :

### A. Resolume envoie de l'OSC — recommandé

Une seule action de l'opérateur pilote les deux logiciels, et aucun câblage MIDI n'est à
partager. Resolume sait émettre de l'OSC quand on lance une colonne ou un clip.

1. **Resolume > Preferences > OSC** : cocher *OSC Output*, destination **Localhost**, port
   **9002** (le port d'écoute du plugin, celui où Max envoie déjà — un même port reçoit les deux,
   c'est de l'UDP).
2. Choisir ce que Resolume émet, dans **Shortcuts > Edit OSC** :
   - le préréglage *Output All OSC Messages* envoie tout. Simple, mais bavard : chaque fader bougé
     part sur le réseau. Sans danger pour le plugin (les adresses inconnues sont ignorées), mais
     le log d'apprentissage en sera plus difficile à lire ;
   - mieux : un préréglage à soi, où seules les colonnes (ou les clips) utiles émettent.
3. **Le plus simple — l'adresse personnalisée.** Dans le même panneau, Resolume permet de donner
   à un élément une *Custom Address*. Donner à la colonne 3 l'adresse `/Cue/Vortex`, et la cue
   nommée « Vortex » dans Unreal se déclenche — **rien à apprendre**, le nom suffit.
4. Sinon, laisser l'adresse d'origine (pour une colonne, de la forme
   `/composition/columns/3/connect` — à vérifier dans le panneau OSC de Resolume, qui affiche
   l'adresse de l'élément cliqué) et l'**apprendre** (§ 4).

Resolume envoie **1 à l'appui et 0 au relâchement**. Le seuil des cues (0,5 par défaut) laisse
passer l'appui et arrête le relâchement : une colonne lancée = une cue jouée.

> Non vérifié avec Resolume lui-même cette nuit : le réglage de sortie OSC de Resolume n'a pas
> été touché (c'est la configuration du spectacle). Les messages ont été simulés avec la forme
> exacte qu'émet Resolume. Voir « J'attends de toi » dans `docs/DECISIONS_NUIT.md`.

### B. Resolume ou Ableton envoie du MIDI vers un port loopMIDI

Utile si une source MIDI existe déjà — par exemple une piste MIDI d'Ableton qui joue des notes à
des instants précis de la bande-son. La source envoie sur un port loopMIDI, Unreal le lit.

### C. Le contrôleur physique lu directement par Unreal

Possible, avec une réserve importante : **sous Windows, une entrée MIDI classique (WinMM) ne
s'ouvre que dans une application à la fois.** Si Resolume tient déjà le Reloop ou le Push, Unreal
ne peut pas l'ouvrir (le log le dit : « déjà ouverte par une autre application »), et
inversement — c'est pourquoi le plugin n'ouvre par défaut **que** les ports nommés « VibH2O ».

Pour que les deux reçoivent le même contrôleur, il faut un logiciel qui le recopie vers un port
loopMIDI (MIDI-OX, Bome MIDI Translator…) — un logiciel de plus à installer et à surveiller. La
voie A rend cela inutile : Resolume tient le contrôleur, Unreal suit Resolume.

---

## 3. loopMIDI

[loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) crée des **ports MIDI
virtuels** : une application écrit dedans, une ou plusieurs autres lisent.

Sur le PC de Dimitri il est **déjà installé et lancé**, avec un port nommé `FromMax`. Ce port a
servi à l'essai de cette nuit ; deux ouvertures simultanées en ont été acceptées.

Pour le spectacle :

1. Ouvrir loopMIDI (icône de la barre des tâches), taper **`VibH2O`** dans *New port-name*,
   cliquer **+**. Le port apparaît pour toutes les applications.
2. Cocher dans loopMIDI le démarrage avec Windows, sinon le port n'existe pas au redémarrage du
   PC. Le plugin réessaie toutes les 5 s d'ouvrir un port absent : lancer loopMIDI après Unreal
   fonctionne aussi.
3. Dans la source (Resolume : *Preferences > MIDI*, Ableton : sortie de la piste), choisir le port
   `VibH2O` en sortie.

Pour lire un autre port, l'ajouter dans **Project Settings > Plugins > VibH2O > Cues > Entrées
MIDI** (une partie du nom suffit, la casse est ignorée ; `*` ouvre tout — voir la réserve du
§ 2 C). `VibH2O.Midi` dans la console liste les ports du PC et ceux qui sont ouverts.

---

## 4. Le directeur de cues

Glisser un **VibH2O Cue Director** dans le niveau (Place Actors, chercher « Cue »). Tout se règle
dans son panneau Details.

### Une cue

| Champ | Rôle |
|---|---|
| **Name** | Nom affiché dans le log ; c'est aussi lui que vise `/Cue/<Name>`. |
| **Source** | OSC, MIDI note, MIDI CC, MIDI program change, touche. Les champs utiles apparaissent selon le choix. |
| Adresse OSC / Canal MIDI (0 = tous) / Numéro MIDI / Touche | L'entrée qui déclenche. |
| **Seuil** | Une entrée valuée ne déclenche qu'au-dessus (0,5 : l'appui passe, le relâchement non). |
| **Continu (fader)** | Déclenche à chaque message, valeur transmise, sans anti-rebond : pour un fader sur `BlendAlpha` ou un attracteur. |
| **Action** | Voir ci-dessous. |

| Action | Effet |
|---|---|
| Séquence — Lancer | Reprend la séquence là où elle en est. |
| Séquence — Lancer depuis le début | Revient au début (sans rejouer les événements intermédiaires), puis lance. |
| Séquence — Pause | |
| Séquence — Arrêter | Arrête ; la tête de lecture revient au début. |
| Séquence — Sauter à un marqueur | Va au marqueur (étiquette tapée dans Sequencer), puis lance ou reste en pause selon *Lancer après le saut*. |
| Séquence — Sauter à un temps | Idem, en secondes. |
| Régler BlendAlpha | Envoie `/Blend <valeur>` au plugin — l'acteur de scène le suit (`bFollowOscBlend`). |
| Message OSC interne | Envoie n'importe quelle adresse au plugin comme si elle venait du réseau : `/Attractor/<nom>/ 1` pour un attracteur, `/Effect/<nom>/ 0` pour couper un effet, `/Focus/Group/`… Toute route de contrôle du plugin est ainsi pilotable sans code de plus. |
| Événement Blueprint seulement | Ne fait rien d'autre que lever `OnCueFired`, pour un Blueprint. |

Chaque cue lève `OnCueFired(Nom, Valeur)`, quelle que soit son action : un Level Blueprint peut s'y
abonner pour tout le reste.

Les séquences pointées sont des **acteurs Level Sequence du niveau** (pipette). Laisser leur
*Auto Play* décoché : c'est la cue qui lance.

### Apprendre

1. Dans le directeur, **Cue à apprendre** : `-1` pour ajouter une cue, ou le numéro d'une cue à
   reprogrammer.
2. Cliquer **Apprendre**. Le champ *État* passe à « En écoute… » et dit ce qui est ouvert (port
   OSC, entrées MIDI, clavier).
3. Dans les 30 s : lancer la colonne dans Resolume, frapper le pad, bouger le fader.
4. *État* affiche ce qui a été appris ; la cue est remplie (source, adresse ou canal et numéro).
   Il reste à choisir son **action**. Ctrl+Z annule.

L'apprentissage se fait **dans l'éditeur, hors Play** : la partie en cours tiendrait déjà le port
OSC. Lancer Play pendant une écoute l'interrompt proprement. Un relâchement (valeur 0) n'est
jamais pris pour l'entrée à apprendre.

**Lister les entrées MIDI** écrit dans *État* les ports MIDI du PC.

### Par la console ou par le nom

- `VibH2O.Cue <nom>` déclenche une cue à la main.
- `/Cue/<nom>` en OSC fait de même, depuis Resolume, Max ou l'iPad.

### Pendant un rendu

`bIgnoreOscControl` sur le subsystem coupe aussi les cues — MIDI et clavier compris. *Cues
actives* sur le directeur (animable dans Sequencer) les coupe pour ce seul directeur.

---

## 5. Réglages de projet (Project Settings > Plugins > VibH2O > Cues)

| Réglage | Défaut | |
|---|---|---|
| Adresse OSC des cues | `/Cue` | `/Cue/<nom>` déclenche par le nom. |
| Lire le MIDI | oui | |
| Entrées MIDI | `VibH2O` | Parties de noms de ports. `*` = tous (réserve § 2 C). |
| Lire le clavier en arrière-plan | **non** | Voir § 6. |
| Pleine vitesse en arrière-plan | oui | Voir § 1. |

---

## 6. Le clavier sans le focus — possible, avec deux risques

Windows offre deux moyens de lire une touche destinée à une autre application :

| Moyen | Effet sur Resolume | Retenu |
|---|---|---|
| `RegisterHotKey` | La touche est **volée** : Resolume ne la reçoit plus. Un raccourci Resolume sur la même touche cesse de marcher. | Non. |
| Raw Input, mode `RIDEV_INPUTSINK` | La touche arrive **aux deux** applications. Un même raccourci peut donc déclencher Resolume et Unreal à la fois. | **Oui**, désactivé par défaut. |

Les deux risques du mode retenu :

1. **Toute frappe compte.** Taper le nom d'un clip dans Resolume, un mot de passe, une note : si
   une touche correspond à une cue, la cue part. D'où : donner aux cues clavier un **modificateur**
   (Ctrl, Maj, Alt), et éviter les touches nues. Une cue sans modificateur exige qu'aucun
   modificateur ne soit tenu : Ctrl+1 ne déclenche pas la cue « 1 ».
2. **Double déclenchement voulu… ou non.** Si Resolume a un raccourci sur la même combinaison, les
   deux partent. C'est utile si c'est voulu, piégeux sinon.

Les touches tenues ne se répètent pas (seul le premier appui compte), et les modificateurs seuls
ne déclenchent rien.

Pour l'activer : cocher **Lire le clavier en arrière-plan**, relancer la partie. Le log confirme :
« cues clavier actives, même quand Unreal n'a pas le focus ».

**Recommandation** : MIDI ou OSC d'abord. Le clavier en secours.

---

## 7. Vérifier sans Resolume

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -run=pythonscript -script="C:/Users/dimit/Documents/GitHub/VIBH2O_UE/Tools/make_cue_test_map.py" -unattended -nosplash
```

Cette commande génère `/Game/Maps/VibH2O_CueTest` : une séquence de 30 s avec un marqueur
« Acte2 » à 10 s, et un directeur portant les cues d'exemple (note 60 → lancer depuis le début,
note 61 → arrêter, colonne 1 → marqueur Acte2, colonne 2 → pause, `/Cue/Vortex` → BlendAlpha 1,
Ctrl+Maj+F12 → lancer).

Jouer la carte en jeu (sous Git Bash, préfixer par `MSYS_NO_PATHCONV=1`, sinon `/Game/...` est
réécrit en chemin Windows et la mauvaise carte s'ouvre), mettre n'importe quelle autre fenêtre au
premier plan, puis :

```bash
python Tools/vibh2o_cue_check.py --midi-port VibH2O --scenario
```

Chaque envoi est daté avec le titre de la fenêtre active ; le log d'Unreal montre en face la cue
jouée et l'état de la séquence (« en lecture à 10,00 s »).

Les règles elles-mêmes sont couvertes par quatre tests d'automation (`VibH2O.Cues.*`) :
correspondance et décodage MIDI, directeur contre un vrai subsystem, vraie Level Sequence pilotée,
apprentissage de bout en bout par un vrai datagramme UDP.

---

## 8. Pourquoi l'entrée MIDI est écrite dans le plugin

Le plugin *MIDI Device Support* d'Epic a été examiné : son API publique est **identique** entre
5.5 et 5.8 (seules les macros d'export changent). Il n'a pas été retenu pour autant :

- il est en **Beta** et désactivé par défaut — exactement le genre de statut qui bouge, et la
  raison pour laquelle le plugin n'utilise pas non plus l'OSC d'Epic ;
- il passe par PortMidi, qui repose sur WinMM : il ne lève pas la limite « une application par
  entrée » ;
- WinMM tient en une centaine de lignes et n'a pas changé depuis vingt ans.

Le fil d'exécution de WinMM n'écrit que dans un tampon préalloué ; toute la logique se fait sur le
game thread — la même règle que pour le récepteur OSC.
