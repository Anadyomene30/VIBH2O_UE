# Décisions prises seul, nuit du 23 au 24 octobre 2026 — à valider

Branche `claude/bulles-look-etats-6977f1` : les trois états de salle et la bulle à stries. Tes
réponses de la soirée sont appliquées telles quelles (stries PPCM éclairées ; couleur, énergie,
mouvement et taille changent avec l'état ; palette d'état plus nuance individuelle ; synchronie en
cohérence ; palettes froid → chaud ; bascule en ~4 s ; panneau rapide avec preset, seuils, forçage,
lumière et taille). Ce qui suit, je l'ai tranché seul. Pour chaque point : le choix, la raison,
l'alternative.

**Captures avant / après :**
`C:\Users\dimit\Documents\GitHub\VIBH2O_UE\Saved\VibH2OSweep_ambiance_2026-10-23\PLANCHE_avant_apres.png`
(et chaque phase en grand dans le même dossier).

---

## 1. Seuils par défaut : calme sous 0,25, excité au-dessus de 0,55

- **Raison.** La plage envoyée par Max n'est pas connue (« je ne sais pas »). Ces valeurs
  correspondent à l'hypothèse actuelle du code (0 → 1, 0 = au repos) et au simulateur. Surtout,
  l'excitation de la salle s'affiche en direct à côté des seuils : on les pose en salle en
  regardant ce chiffre.
- **Alternative.** Une excitation signée (0 = baseline) : régler `ExcitationInputMin` à −1 dans les
  Project Settings, et les seuils autour de 0,5.

## 2. Anti-clignotement : marge de ±0,04, lissage de 1,5 s, fondu de 4 s

- **Raison.** Testé : une minute de bruit de ±0,08 à cheval sur un seuil ne fait **aucun**
  changement d'état ; une montée lente en fait exactement deux.
- **Alternative.** Une durée minimale dans chaque état. Inutile tant que la marge suffit.

## 3. La nuance individuelle se lit par rapport à la salle, pas dans l'absolu

- **Raison.** La moyenne de la salle tombe au milieu de la palette ; une personne plus calme tire
  vers la première couleur, une plus excitée vers la seconde. Chaque palette sert ainsi en entier,
  même quand toute la salle est haute ou basse. Contraste réglable (2,5 par défaut).
- **Alternative.** Placer la bulle selon son excitation absolue. Dans une salle excitée, toutes les
  bulles prendraient alors la même couleur.

## 4. Cohérence = moitié synchronie de la salle, moitié synchronie de la personne

- **Raison.** Dans une salle synchrone, une personne désynchronisée se détache.
- **Alternative.** `Part de la synchronie individuelle` à 0 (la salle seule) ou à 1 (chacun la
  sienne). Ce réglage est sous la flèche avancée.

## 5. Le matériau est en Subsurface **masked**, deux faces, avec un tramage temporel

- **Raison.** C'est le principe de la bulle PPCM (`Bubble_Mat` : masked + DitherTemporalAA). Le
  matériau reste opaque pour l'éclairage, donc il reçoit ombres et caustiques, comme l'exige la
  spec. Les deux faces laissent voir les bandes de l'autre côté.
- **Alternative.** Opaque, avec une transparence feinte par le seul Fresnel : `Transparency` à 0
  dans le matériau. Le tramage se voit un peu en image fixe (grain) ; en mouvement, l'antialiasing
  temporel le lisse.

## 6. Niveaux d'émissif minuscules, calés sur une exposition manuelle à +10 EV

- **Raison.** La carte de démo est à +10 EV et PPCM à +10,5. Un émissif de 1 y brûle tout en blanc :
  c'était le défaut de l'ancien plugin dans PPCM. Le nœud `EyeAdaptationInverse`, essayé pour
  rendre l'émissif indépendant de l'exposition, n'a rien changé de mesurable.
- **Alternative.** Dans une scène exposée autrement, régler `Lumiere des bulles` sur l'acteur de
  scène, ou `StripeLight`, `RimLight` et `HeartLight` dans le matériau.

## 7. Couper l'effet « teinte » coupe aussi la couleur d'état

- **Raison.** `VibH2O.EffectIsolation` a montré que, sinon, couper la teinte ne faisait plus rien
  dans une salle uniforme. Coupé, il ne reste qu'une couleur fixe : le milieu de la palette
  normale.
- **Alternative.** Garder la couleur d'état et ne couper que la nuance individuelle.

## 8. `CalmColor`, `ExcitedColor` et `ExcitationGradient` sont supprimés de l'acteur de scène

- **Raison.** Les palettes par état les remplacent. Les garder laisserait deux systèmes de couleur
  concurrents dans le panneau. Aucune carte du dépôt, ni le script de composition LUX, ne les
  réglait.
- **Alternative.** Les rétablir comme repli « sans palette ». Point à vérifier : une carte LUX
  qui les aurait réglées à la main perdrait ces valeurs (avertissement au chargement, sans plantage).

## 9. L'acteur joue sa propre copie des palettes ; le preset sert à charger et à enregistrer

- **Raison.** Sequencer ne peut animer que les propriétés de l'acteur, pas celles d'un asset. Un
  réglage dans Details se voit tout de suite. Glisser un preset le charge ; le bouton
  « Enregistrer dans le preset » y réécrit le réglage.
- **Alternative.** Lire le preset en direct. Sequencer ne pourrait alors plus animer les couleurs.

## 10. Pas d'essai dans une copie de PPCM cette nuit

- **Raison.** L'éditeur LUX était ouvert par une autre session (pluie). Le plugin du projet LUX
  est partagé : sa DLL a été remplacée à 22 h 48 par le build d'une autre branche. Y installer le
  mien aurait écrasé le travail des autres.
- **Alternative.** Le faire après la fusion des branches : installer le plugin fusionné dans LUX,
  puis ouvrir `VIBH2o_PPCM_VibH2O` (la copie 1, jamais `VIBH2o_PPCM`).

## 11. Seul le matériau de bulle est régénéré

- **Raison.** Dans ce worktree, `make_demo_material.py` plante à la recréation des caustiques,
  **après** avoir écrit la bulle. Les caustiques, le fond et les particules relèvent de la session
  `environnement` ; je n'ai commité que `M_VibH2ODemoBubble`.
- **Alternative.** Corriger le script dans la branche `environnement`, puis tout régénérer.

## 12. Nouveau scénario du simulateur plutôt que modifier « Vague d'excitation »

- **Raison.** La vague existante traverse la salle sans en changer la moyenne : elle ne fait jamais
  changer d'état. Le nouveau scénario, « Vague collective : calme, normal, excite » (cycle de 96 s,
  synchronie sur 37 s), passe par les trois. L'ancien reste intact.
- **Alternative.** Élargir la vague existante pour qu'elle couvre la salle.

# Décisions de la nuit — attracteurs, zones d'exclusion, télécommande iPad

Session `attracteurs-ipad`, branche `claude/attracteurs-ipad-7f55c7`, 23 octobre 2026. Les six premiers
points ont été tranchés **par Dimitri** en début de session ; les suivants l'ont été en autonomie, à
valider au réveil. Chacun porte sa raison et l'alternative écartée.

## Tranchés par Dimitri

| Question | Choix | Alternative écartée |
|---|---|---|
| Forme de l'appli iPad | Web app servie **par le plugin lui-même**, installée en plein écran depuis Safari | Relais Python ; vraie appli iOS (Xcode + compte Apple, 7 jours sans compte) ; Remote Control d'Epic |
| Qui va vers un attracteur | **Une part réglable**, toujours les mêmes bulles | Toute la salle ; un groupe de focus |
| Zones d'exclusion | **Volumes 3D** placés dans la scène | Zones 2D sur l'image du tulle |
| Mobilité des zones | **Déplaçables en direct** depuis l'iPad | Fixes activables ; toujours actives |
| À l'arrivée | **Un nuage qui respire** | Tourbillon ; point serré |
| Plusieurs attracteurs | **Cumulables** | Exclusifs |
| Durée des fondus | **Réglée dans l'éditeur + curseur iPad** | Éditeur seulement |
| En plus sur l'iPad | Tout relâcher, Blend, focus, effets | — |

## Tranchés en autonomie — à valider

1. **Le fondu est un ressort critique, pas une rampe.** Départ et arrivée sans à-coup, et surtout un
   contre-ordre à mi-course ne fait ni saut ni rebond. *Alternative :* rampe adoucie par un smoothstep,
   plus prévisible en durée mais qui rebondit visiblement si l'on change d'avis en route. La durée
   affichée est le temps pour couvrir 99 % du chemin.
2. **Un ressort par bulle et par attracteur, durées étalées** (`Etalement` 0,6). La salle part comme un
   flux. *Alternative :* un seul poids par attracteur — un bloc rigide, et un changement de part en
   direct ferait sauter des bulles.
3. **Les zones contournent au lieu de projeter.** Chaque bulle retient le côté par lequel elle est
   poussée, qui ne tourne qu'à `Vitesse de contournement` : une bulle qui traverse la zone glisse le
   long du bord. *Alternative :* projection pure sur le bord le plus proche, sans état — elle saute d'un
   bord à l'autre au milieu de la zone, donc devant la circassienne (prouvé par le test témoin).
4. **Zone poussée de côté seulement**, jamais en profondeur, et **sur toute la profondeur** par défaut :
   ce qui compte est ce qui tombe sur la circassienne à l'image. Avec une *Camera de projection*
   désignée, la zone devient un cône depuis l'objectif. *Alternative :* boîte 3D stricte — laisserait
   passer une bulle lointaine qui la recouvre pourtant à l'image.
5. **Port 8642**, et non 9080 envisagé d'abord : Logitech G HUB occupe 127.0.0.1:9080 sur ce PC, et
   Windows laisse un autre programme écouter 0.0.0.0:9080 sans erreur. Une sonde au démarrage l'écrit
   dans le log. *Alternative :* un port UDP pour TouchOSC — inutile, une page web ne parle pas UDP.
6. **HTTP en interrogation (≈ 3 fois par seconde), pas de WebSocket.** Le serveur reste minuscule et
   se reconnecte tout seul après une coupure Wi-Fi. *Alternative :* WebSocket écrit à la main (SHA-1,
   trames) — plus de code réseau à maintenir pour un gain imperceptible sur des boutons.
7. **Les commandes de l'iPad sont des lignes OSC** (`/Attractor/Gauche/ 1`), injectées dans la même
   file que Max. Un seul chemin de contrôle ; `bIgnoreOscControl` les coupe aussi. *Alternative :* une
   API propre à l'iPad — un deuxième chemin à garder cohérent.
8. **Zones actives par défaut**, attracteurs inactifs par défaut. Une zone posée protège aussitôt.
   *Alternative :* tout inactif — mais une zone oubliée éteinte laisse passer les bulles devant une
   artiste, le pire des deux oublis.
9. **Attracteurs et zones trouvés chaque image dans le niveau**, sans liste à remplir sur la scène.
   *Alternative :* une liste dans l'acteur de scène — une étape de plus à la mise en place, et une
   source d'oubli.
10. **Nom de contrôle tiré du nom de l'acteur**, mais jamais réécrit une fois tapé à la main.
11. **La page est livrée hors du pak**, à côté de l'exécutable packagé, modifiable sur place et
    rechargée par `VibH2O.Restart`. *Alternative :* dans le pak — intouchable sans repackager.
12. **Bouton « Fond » ajouté** à l'iPad à la demande relayée par la session `environnement` : il envoie
    `/Fond 1|0`. L'état publié ne connaissant pas encore le fond sur cette branche, le bouton se souvient
    de la dernière commande envoyée ; après fusion, publier `backdrop` (0 à 1) dans
    `BuildRemoteStateJson` suffira pour qu'il reflète l'état réel.
13. **Carte de démo** : deux attracteurs (tiers de salle disjoints, attachés à la scène) et une zone
    *Circassienne* liée à la caméra. Carte régénérée et commitée ; en cas de conflit binaire à la
    fusion, la régénérer plutôt que choisir un côté.
14. **Pas d'authentification** sur la télécommande : le réseau du spectacle est fermé. *Alternative :*
    un code PIN — à ajouter si le Wi-Fi est partagé avec le public.

## Ce que je n'ai pas pu faire

- **L'iPad réel** : ajout à l'écran d'accueil, lisibilité dans le noir, maintien de l'écran allumé.
  Vérifié seulement dans un navigateur au format iPad (1180 × 820).
- **Les vraies circassiennes** : placement des zones et jugement à l'image dans le projet LUX — je n'ai
  pas ouvert l'éditeur LUX.

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

# Décisions de la nuit — branche `claude/flock-division-e9c6e8`

Division des bulles et banc en vortex (tableau 2). Les six choix artistiques de départ ont été
**validés par Dimitri** avant le code (mitose égale, division entre BlendAlpha 0 et 0,35, familles
lisibles hors synchronie, ~700 bulles, fondu entre deux seuils, cône évasé). Ceux qui suivent ont
été **pris seul**, faute de réponse : chacun est à confirmer ou à changer au réveil.

| # | Décision prise | Raison | Alternative | Où changer |
|---|---|---|---|---|
| 1 | **Les filles sont des bulles à part entière** (acteurs), pas des meshes instanciés ni du Niagara. | Mesuré : 704 bulles tiennent 120 à 145 images/s en rendu réel. Les filles gardent le même matériau, le même battement et les mêmes événements Blueprint que leur mère. Les instances rendaient noir le matériau des particules. | Meshes instanciés : moins coûteux au-delà de ~1 500 bulles, mais avec un autre contrat matériau. | Code. Le `Plafond du banc` (1 000) protège les images/s en attendant. |
| 2 | **Les filles déclenchent `Sur battement`** comme leur mère. Un booléen `Is Daughter` permet de les filtrer. | Le battement d'une famille se voit sur toutes les sœurs. Le son, lui, se filtre en une ligne dans le Blueprint. | Ne faire battre que la mère : les sœurs paraîtraient éteintes. | Blueprint de la bulle. |
| 3 | **Le numéro de siège ne s'affiche que sur la mère.** | Un numéro par spectateur reste lisible ; quatre ne le seraient plus. | Numéroter chaque sœur. | Code (`SetSeatLabelVisible`). |
| 4 | **En vortex, les familles se fondent dans le banc.** Elles se reforment dès que la synchronie retombe. | C'est l'image que Dimitri a choisie : « en synchronie, les familles se fondent dans un seul vortex ». | Garder les familles groupées même dans le vortex : le cône serait moins net. | `Kin Weight` du `Flock Driver` (effet hors synchronie seulement). |
| 5 | **Volume de dispersion de 1 100 × 800 cm** (700 × 620 auparavant). | Avec 176 familles, l'ancien volume les entassait. Les familles sont nettes en périphérie, encore serrées au centre (capture `04_banc_disperse`). | Plus grand (1 400 cm) : plus lisible, mais le banc dépasse le cadre de la salle. | `Bounds Radius` / `Bounds Height` du `Flock Driver`. |
| 6 | **Vitesses de formation** : le vortex se forme en ~5 s, les familles se reforment en ~6 s. | Le balayage du simulateur laisse ~10 s à chaque état ; c'est suffisant pour qu'il se lise. | Plus vif : `Lissage` à 1 s. Plus lent et plus solennel : `Lissage` à 4 s. | `Lissage`, catégorie `Tableaux|Banc`. |
| 7 | **Le sens de rotation suit le signe de `Vortex Spin Speed`**, un réglage déjà existant. | Ce réglage avait déjà ce rôle pour l'amorce de vortex ; pas de réglage en plus. | Un booléen dédié. | `Vortex Spin Speed`. |
| 8 | **Les poids du pilote ont été réglés par mesure** : cohésion de voisinage quasi nulle, alignement réduit en vortex, force centripète exacte. | À l'ancien réglage, le vortex s'effondrait en boule au bout de 15 s (rotation 0,25). Au nouveau : rotation 0,98, 100 % des bulles sur le cône. | Aucune qui tienne la forme : c'était mesuré. | `Flock Driver`, catégories `Banc|Vortex`. |

Chiffres et méthode : [`ROADMAP.md`](ROADMAP.md), section *Tableau 2 enrichi*. Pour juger à l'œil :
`VibH2O.SchoolSweep` (une minute et demie, captures dans `Saved/VibH2OSchool/`).
