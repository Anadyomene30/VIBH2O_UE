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
