# Décisions de la nuit du 23 au 24 octobre 2026 — cues MIDI / OSC / clavier

Dimitri dormait : chaque choix ci-dessous a été tranché seul, sur la recommandation que je
t'aurais faite. Chacun dit **ce qui a été choisi**, **pourquoi**, et **l'alternative** si tu
préfères l'autre voie. Le guide d'emploi est [`CUES.md`](CUES.md).

Branche `claude/cues-midi-061e49`, pas de push, pas de PR.

---

## 1. Resolume pilote, Unreal suit — par l'OSC d'abord

**Choix.** La voie recommandée est que Resolume émette de l'OSC à chaque colonne lancée, vers le
port 9002 d'Unreal. Une seule action de l'opérateur pilote les deux. Le plus simple : donner aux
colonnes utiles une *Custom Address* `/Cue/<nom>` dans Resolume — aucune cue à apprendre.

**Pourquoi.** L'OSC arrive sans le focus, ne partage aucun périphérique, et le plugin a déjà son
récepteur. Le MIDI garde sa place pour une source qui existe déjà (Ableton, un pad), par un port
loopMIDI.

**Alternative.** Tout en MIDI : Resolume envoie ses retours MIDI vers un port loopMIDI. Ça
marche aussi (les cues MIDI sont prêtes), mais c'est un câblage virtuel de plus à maintenir.

## 2. Entrée MIDI écrite dans le plugin (WinMM), pas le plugin MIDIDevice d'Epic

**Choix.** Une centaine de lignes sur WinMM, l'API MIDI système de Windows.

**Pourquoi.** J'ai comparé les sources de *MIDI Device Support* en 5.5 et 5.8 : l'API publique
est identique (seules les macros d'export changent), donc il *aurait* tenu la contrainte « un seul
code ». Mais il est en **Beta**, désactivé par défaut, et repose lui-même sur WinMM via PortMidi —
il n'apporte rien de plus, et son statut est exactement ce qui a fait écarter l'OSC d'Epic.

**Alternative.** Basculer sur MIDIDevice : une dépendance de plugin dans le `.uplugin`, et
l'abonnement à `OnMIDIRawEvent`. Le reste (cues, directeur) ne changerait pas.

## 3. Par défaut, le plugin n'ouvre que les ports MIDI nommés « VibH2O »

**Choix.** Réglage *Entrées MIDI* = `VibH2O`. Il faut donc créer un port loopMIDI de ce nom.

**Pourquoi.** Sous Windows, une entrée MIDI classique ne s'ouvre que dans **une** application à
la fois. Tout ouvrir par défaut risquerait de prendre le Reloop ou le Push à Resolume si Unreal
démarre en premier — une panne silencieuse en représentation. Un port loopMIDI, lui, accepte
plusieurs lecteurs (vérifié : deux ouvertures simultanées de `FromMax` acceptées).

**Alternative.** Mettre `*` (tout ouvrir), ou ajouter `FromMax` ou le nom du contrôleur à la liste.

## 4. Clavier sans le focus : Raw Input non bloquant, désactivé par défaut

**Choix.** `RIDEV_INPUTSINK` (la touche arrive à Resolume **et** à Unreal), jamais
`RegisterHotKey` (qui la volerait à Resolume). Réglage *Lire le clavier en arrière-plan* décoché.

**Pourquoi.** C'était ta demande, et la seule façon de lire le clavier sans le focus sans casser
les raccourcis de Resolume. Mais toute frappe dans Resolume peut alors déclencher une cue : d'où
le défaut à « non », et des modificateurs (Ctrl/Maj/Alt) sur les cues clavier. Une cue sans
modificateur exige qu'aucun ne soit tenu.

**Vérifié** : Ctrl+Maj+F12 synthétisé avec Resolume au premier plan a déclenché la cue, Resolume
gardant le focus.

**Alternative.** `RegisterHotKey` si tu veux qu'une touche soit **réservée** à Unreal — à écrire,
une vingtaine de lignes.

## 5. Les cues vivent dans un acteur du niveau, pas dans un asset

**Choix.** `AVibH2OCueDirector`, avec sa liste de cues dans le panneau Details.

**Pourquoi.** Les cues visent des acteurs Level Sequence **du niveau** : un acteur du même niveau
les référence directement, à la pipette. Un Data Asset ne peut pas pointer un acteur de niveau.

**Alternative.** Une table en Data Asset qui viserait les séquences par asset plutôt que par
acteur — réutilisable entre niveaux, mais moins directe.

## 6. Une seule entrée pour tous : le subsystem

**Choix.** Le subsystem, qui possède déjà le récepteur OSC, possède aussi le MIDI et le clavier,
et publie un flux unique `OnCueInput`. Les messages de plan de salle et de données capteurs n'y
entrent jamais ; tout le reste — y compris les routes `/Blend`, `/Effect`, et `/Attractor` de la
session attracteurs-ipad — oui.

**Pourquoi.** Une seule règle de threading pour les trois entrées, et **pas de deuxième récepteur
OSC** : la session attracteurs-ipad (qui ajoute ses routes et un serveur HTTP pour l'iPad) n'a
rien à adapter. Ses attracteurs se pilotent par une cue « Message OSC interne » vers
`/Attractor/<nom>/`.

**Garde-fou.** Une cue qui renvoie le message qu'elle écoute (`/Blend` → `/Blend`) ne s'appelle
pas elle-même à l'infini : testé.

## 7. Apprendre : dans l'éditeur, hors Play

**Choix.** Le bouton *Apprendre* ouvre le port OSC et les entrées MIDI le temps de la capture
(30 s au plus), prend la première entrée non nulle, referme.

**Pourquoi.** Un apprentissage en Play serait perdu à l'arrêt de Play. Hors Play, le subsystem
éditeur n'écoute jamais (pour ne pas voler le port à Play) : la capture l'ouvre juste le temps
qu'il faut, et Play l'interrompt proprement.

**Défaut trouvé et corrigé en route** : la capture se refermait au milieu du vidage de la file
réseau, détruisant le récepteur en pleine boucle — un plantage. Les boucles revérifient désormais
le récepteur à chaque tour, et un test d'apprentissage de bout en bout (vrai datagramme UDP) le
couvre.

## 8. Pleine vitesse en arrière-plan, activé par défaut

**Choix.** Le plugin remet `t.IdleWhenNotForeground` à 0, lève le bridage CPU de l'éditeur
**pendant Play seulement**, et retire le processus du bridage d'énergie de Windows 11.

**Pourquoi.** Sans cela les cues arriveraient mais le Spout saccaderait. *Use Less CPU when in
Background* est déjà décoché dans le projet LUX, mais rien n'empêche qu'il soit recoché un jour ;
et le bridage d'énergie de Windows n'a aucun réglage dans Unreal.

**Ce que le plugin ne fait pas** : il ne décoche pas lui-même la préférence de l'éditeur (c'est un
réglage de l'utilisateur) ; il l'écrit dans le log si elle est cochée.

## 9. Ce qui n'a pas été fait exprès

- **La configuration OSC de Resolume n'a pas été touchée** : c'est le réglage du spectacle. Les
  messages de Resolume ont été simulés à l'identique.
- **Aucun logiciel installé** : loopMIDI était déjà là ; aucun port créé (le port `VibH2O` est à
  créer, § 3 de `CUES.md`).
- **Projet LUX non touché** : ni carte, ni plugin réinstallé. L'essai s'est fait dans le projet
  du dépôt, en jeu, avec Resolume Arena au premier plan.
- **Pas de fondu temporel dans les cues** (« aller au vortex en 8 s ») : une Level Sequence le fait
  déjà, et la cue la lance.

---

## Ce que j'attends de toi au réveil

1. **Valider ou changer les choix 1 à 8.** Aucun ne demande de reprendre le code pour changer
   d'avis, sauf le 2 et le 5.
2. **Un essai avec Resolume pour de vrai** — le seul maillon non vérifié :
   1. créer le port loopMIDI `VibH2O` (§ 3 de `CUES.md`) ;
   2. dans Resolume, *Preferences > OSC* : sortie vers Localhost, port 9002 ; dans
      *Shortcuts > Edit OSC*, donner à une colonne l'adresse personnalisée `/Cue/Note60` ;
   3. générer et jouer la carte `VibH2O_CueTest` (§ 7 de `CUES.md`), revenir dans Resolume ;
   4. lancer la colonne. **Tu dois voir** dans le log d'Unreal : `cue 'Note60' -> Sequence -
      Lancer depuis le debut` puis `sequence 'LS_VibH2O_CueTest' en lecture`.
   5. Me répondre : « ça marche », ou la ligne de log reçue (avec `bLogInputs`, chaque entrée est
      écrite, même sans cue correspondante).
3. **Installer le plugin dans le projet LUX** quand tu le décides : la session environnement-fond
   y a déjà installé un build ; il faut fusionner les branches avant de réinstaller (voir les
   notes partagées), sinon l'acteur `VibH2O_Fond` de `VIBH2o_PPCM_Fond` serait perdu.
