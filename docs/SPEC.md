# Spécification fonctionnelle — plugin VIBH2O

Ce que le plugin doit savoir faire. Le **comment** et le **pourquoi** sont dans
[`../CLAUDE.md`](../CLAUDE.md) ; le protocole réseau dans
[`OSC_PROTOCOL.md`](OSC_PROTOCOL.md) ; l'ordre de construction dans
[`ROADMAP.md`](ROADMAP.md).

**Périmètre de la première livraison :** tout ce qui suit, sauf les deux exclusions listées en
fin de document.

---

## A. Réseau et protocole

1. **Écouter l'OSC en UDP** sur une IP et un port réglables dans les Project Settings
   (défaut `9002`), sur un thread séparé.
2. **Parser l'OSC 1.0** : messages et bundles, types entier / flottant / chaîne, padding à
   4 octets, tolérance au slash final, résistance aux trames tronquées.
3. **Transférer au game thread par file d'attente** — aucun accès à un `UObject` depuis le thread
   réseau.
4. **Rendre toutes les adresses OSC configurables** sans recompilation.

## B. Le plan de salle

5. **Recevoir le mapping** : colonnes, rangées, et un identifiant d'individu par index de siège.
6. **Attendre la fin de la rafale** (~100 ms de silence) avant de construire.
7. **Traiter l'identifiant 0 comme un siège vide** : aucune bulle, exclusion du routage et de
   toutes les moyennes.
8. **Basculer l'ordre de lecture** rangées / colonnes, pour lever la transposition.
9. **Reconstruire par différence** : conserver les bulles inchangées, ne créer et détruire que ce
   qui bouge — sinon toute la salle clignote à chaque envoi depuis Max.

## C. Les données individuelles

10. **Router** BPM, excitation et synchronie vers la bonne bulle, par identifiant.
11. **Dériver le battement du BPM** par accumulateur de phase, sans saut au changement de tempo.
12. **Garder une entrée d'événement de battement** câblée mais inutilisée, pour le jour où Max
    l'enverra.
13. **Calculer la synchronie collective**, en excluant sièges vides et capteurs muets.
14. **Détecter les capteurs muets** au-delà d'un délai réglable (défaut 5 s).

## D. Géométrie de la salle

15. **Placer les bulles en grille**, espacements X / Y et hauteur réglables.
16. **Courber la salle** dans les deux sens, avec retour exact à la grille droite à zéro.
17. **Choisir entre espacement constant et éventail** — l'éventail reproduisant la géométrie d'un
    vrai théâtre, où les rangées du fond s'élargissent.
18. **Orienter les bulles** vers le centre de courbure.
19. **Élever les rangées** pour reproduire des gradins.
20. **Varier la profondeur** par un bruit doux, pour que la salle ait du relief sans devenir
    illisible.
21. **Tout calculer en espace local**, pour que l'installation reste déplaçable dans le niveau.

### Réglages de géométrie

| Réglage | Rôle |
|---|---|
| `CurvatureAngle` | Angle **signé** par pas de colonne, en degrés. **0 = salle droite.** Positif ou négatif pour courber dans un sens ou dans l'autre. |
| `bFanOut` | Espacement constant (grille régulière) ou éventail (angle constant). |
| `bOrientToCenter` | Les bulles pivotent vers le centre de courbure. |
| `OrientationOffset` | Décalage de rotation supplémentaire. |
| `ElevationPerRow` | Décalage vertical par rangée. Courbe optionnelle pour une pente non linéaire. |
| `DepthAmplitude` | Amplitude du décalage de profondeur, en cm. |
| `DepthNoiseScale` | Échelle du bruit générant ce décalage. |

**Deux points à ne pas rater.** L'angle se paramètre en **degrés par siège** et non en rayon : une
salle droite correspondrait à un rayon infini, impossible à saisir. Et l'**orientation n'est pas
un ornement** — les bulles portent des stries directionnelles ; si elles gardaient toutes le même
cap sur une salle courbe, les stries pointeraient toutes dans la même direction et trahiraient
immédiatement la courbure.

**La profondeur est tirée d'un bruit, pas d'un aléatoire par siège.** Un aléatoire pur décorrèle
les voisins et détruit la lecture des rangées ; un bruit fait onduler la nappe en gardant les
rangées identifiables. Le décalage doit être **déterministe** (dérivé de l'index de siège), donc
stable d'un lancement à l'autre et d'une reconstruction à l'autre.

## E. Mouvement

22. **Faire flotter les bulles en permanence**, en translation et en légère rotation, chacune avec
    sa propre phase.
23. **Faire dériver les bulles** selon l'excitation, au-delà d'un seuil réglable, **sans jamais
    sortir de leur case**.
24. **Mesurer la vitesse réelle** de chaque bulle, pour piloter l'animation des stries.
25. **Amorcer le vortex** du tableau 2 et **mélanger les deux tableaux** par un curseur continu.

### La pile de position

Évaluée par bulle et par frame, en espace local :

```
Local = Lerp( Grille.Eval(i) , Flock.Eval(i) , BlendAlpha )   ← le tableau
      + Flottement(i)                                          ← toujours actif
```

**Le flottement s'ajoute après le mélange**, et non dans le pilote de grille : c'est une propriété
de la bulle — elle est sous l'eau, donc elle flotte — et non du tableau. Il survit ainsi au
passage au vortex.

**Chaque bulle a sa propre phase de flottement**, dérivée de son index. Sans cela, toutes les
bulles ondulent à l'unisson et l'ensemble paraît mécanique.

**La dérive est bornée par construction** : amplitude
`saturate((Excitation - Seuil) / (1 - Seuil)) * Intensité`, puis limitation stricte à une fraction
de l'espacement entre sièges. Sous le seuil, immobilité totale ; au-dessus, la bulle ne peut
mathématiquement pas atteindre la case voisine.

| Réglage | Rôle |
|---|---|
| `FloatAmplitude` / `FloatSpeed` | Amplitude et vitesse du flottement permanent. |
| `FloatRotationAmount` | Amplitude du tangage lent qui l'accompagne. |
| `DriftThreshold` / `DriftIntensity` | Seuil de déclenchement et intensité de la dérive. |
| `DriftMaxRatio` | Fraction de l'espacement que la dérive ne peut pas dépasser. |

## F. Rendu

26. **Pousser les paramètres au matériau**, via une instance dynamique créée sur le mesh désigné
    par le Blueprint.
27. **Émettre les événements Blueprint** (battement, mise à jour des données), pour le son, Niagara
    et les particules.
28. **Fournir une bulle de démonstration** — fonctionnelle, et non une direction artistique.

### Paramètres poussés au matériau

| Paramètre | Contenu |
|---|---|
| `BeatPhase` | 0→1, **continu, ne saute jamais** — le socle de tout ce qui est périodique |
| `BeatPulse` | Enveloppe décroissante depuis le dernier battement |
| `Bpm` | Valeur brute |
| `Excitation` | 0→1, normalisée par Max |
| `Synchrony` | 0→1 |
| `StriationSpeed` | Vitesse d'animation des stries, dérivée de la vitesse réelle de la bulle |
| `FocusMask` | 1 dans le groupe en focus, 0 assombri |
| `Staleness` | 1 si le capteur est muet |
| `TintColor` | Couleur issue du gradient d'excitation |

**Les anneaux concentriques** de la référence artistique n'ont besoin d'aucun état : dans le
matériau, `frac(BeatPhase * N - Distance * K)` produit un train d'anneaux qui s'étendent en
permanence, calés sur le cœur réel. C'est la continuité de `BeatPhase` qui garantit qu'ils ne
glitchent jamais lors d'un changement de BPM.

### Direction artistique

Organique, épuré, en mouvement. Référence : une cloche de méduse translucide traversée d'anneaux
concentriques, parcourue de stries radiales fines, dans une lumière bleue diffuse. L'ensemble est
plongé dans un environnement **sous-marin** (`underwater_bp`) et doit recevoir les **caustiques**.

**Le matériau doit être opaque ou masked en subsurface**, la transparence étant feinte par du
Fresnel — un matériau translucide ne recevrait correctement ni les ombres ni les caustiques.
L'émissif module l'éclairage, il ne le remplace pas.

## G. Focus

29. **Définir des groupes nommés** dans le panneau Details : zone rectangulaire, plus une liste de
    sièges isolés pour les sélections éparses.
30. **Sélectionner un groupe en direct**, par Blueprint, par entrée clavier ou par OSC.
31. **Assombrir progressivement** les bulles hors du groupe, avec une intensité continue. Rien ne
    se déplace : le plan de salle reste lisible.
32. **Exposer la cible de cadrage** sans piloter la caméra :

```
GetFocusBounds()  -> FBox      bornes du groupe, en monde
GetFocusCenter()  -> FVector
GetFocusFitDistance(float HorizontalFOV) -> float
OnFocusGroupChanged  (délégué)
```

Ces bornes se calculent depuis les **positions réellement occupées**, jamais depuis une grille
plate supposée — sinon le cadrage vise à côté dès que la salle est courbée.

## H. Pilotage

33. **Activer et désactiver chaque effet** pour toute la salle — la portée est **globale**.
34. **Rendre tous les paramètres animables dans Sequencer**, via le spécificateur `Interp`.
35. **Arbitrer OSC et Sequencer**, avec un réglage pour couper l'OSC pendant un rendu.
36. **Exposer chaque commande par trois chemins** : panneau Details, Blueprint, et OSC. Le MIDI
    passe aujourd'hui par Max ; un pilotage MIDI direct pourra se brancher plus tard sur les mêmes
    fonctions.

## I. Outillage

37. **Simulateur OSC** couvrant tous les cas de test — **partir de `Scripts/OSC_SIMULATOR.py`**
    du dépôt `VibH2o`, qui simule déjà 176 capteurs.
38. **Tests du parseur** exécutables hors moteur.
39. **Projet Unreal de test** avec une bulle de démonstration et une carte.
40. **README** couvrant l'installation, les réglages et le contrat matériau.

---

## Hors périmètre de cette livraison

**Le comportement de banc du tableau 2.** Le vortex livré donne la **forme** — le cône, son
évasement, son resserrement selon la synchronie collective — ce qui suffit à rendre la transition
testable. La véritable logique de banc (voisinage, séparation, alignement, cohésion) reste à
faire.

**Le look définitif des bulles.** Le plugin fournit une bulle de démonstration qui prouve que les
données circulent. Le mesh, le matériau subsurface, les stries et les anneaux relèvent du travail
artistique.

---

## Points ouverts

Aucun ne bloque le démarrage — tous sont rendus configurables pour que le code n'ait pas à être
repris.

- **Les adresses exactes des données live** côté Max, à confirmer sur le patch.
- **La plage de l'excitation normalisée** — on suppose 0→1 — et celle du BPM.
- **Le sens de parcours des sièges**, que seul le test 7 × 3 peut trancher.
- **Le dépôt d'accueil définitif** : `VIBH2O_REBORN` est décrit comme la réécriture Unreal.
