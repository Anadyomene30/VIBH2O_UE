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
