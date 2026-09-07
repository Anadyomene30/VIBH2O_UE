# Tutoriel — se servir du plugin VibH2O

Pour prendre en main l'installation sans rien brancher, puis y greffer son propre travail
artistique. Aucune connaissance du code n'est requise.

> Le **pourquoi** des décisions est dans [`../CLAUDE.md`](../CLAUDE.md), la référence technique
> dans [`../Plugins/VibH2O/README.md`](../Plugins/VibH2O/README.md).

---

## 1. Voir la salle vivre — deux minutes

1. Ouvrir `VIBH2O_UE.uproject`.
2. Ouvrir la carte **`Content/Maps/VibH2O_Demo`** (elle s'ouvre seule au démarrage).
3. Appuyer sur **Play**.

Vingt et une bulles apparaissent, chacune battant au rythme de son propre cœur. Elles se colorent
du bleu à l'orange selon l'excitation, flottent, et portent des anneaux calés sur leur battement.

**Rien n'est branché** : un acteur `VibH2O — Simulateur` est posé dans la carte et joue le rôle du
patch Max/MSP. C'est lui qui invente la salle et les données.

### Le mode qui sert vraiment : Simulate

Plutôt que Play, appuyer sur **Alt + S** (*Simulate*). Même chose à l'écran, mais **vous gardez la
main sur l'éditeur** : vous pouvez sélectionner des acteurs, changer leurs réglages et voir l'effet
immédiatement, sans être enfermé dans la caméra du jeu.

**C'est le mode à utiliser pour régler.** Play ne sert qu'à voir depuis la caméra du spectacle.

### Et sans rien lancer du tout

L'acteur de scène se dessine aussi **dans le viewport de l'éditeur**, sans Play ni Simulate : une
sphère par siège, avec une flèche d'orientation. Pratique pour régler la forme de la salle. Ce sont
des repères de débogage, pas des bulles — pas de matériau, pas de vraies données.

---

## 2. Régler la salle

Sélectionner l'acteur **`VibH2O_Stage`** dans l'Outliner. Le panneau Details se remplit de
catégories `VibH2O|…`. Les plus utiles, dans l'ordre où on s'en sert :

### `VibH2O|Geometrie` — la forme de la salle

| Réglage | Ce qu'il fait |
|---|---|
| `Column Spacing` / `Row Spacing` | Écartement des sièges, en centimètres. |
| **`Curvature Angle`** | Courbe la salle. **0 = parfaitement droite.** Positif ou négatif pour courber d'un côté ou de l'autre. |
| `bFan Out` | Coché : les rangées du fond s'élargissent, comme dans un vrai théâtre. Décoché : écartement constant partout. |
| `bOrient To Center` | Les bulles pivotent vers le centre de la courbe. |
| `Elevation Per Row` | Gradins : hauteur gagnée à chaque rangée. |
| `Depth Amplitude` | Relief : ondulation douce de la nappe de bulles. |

> **Faites l'essai qui fait tout comprendre.** Attrapez `Curvature Angle` et balayez-le du négatif
> au positif, directement dans le viewport, sans rien lancer. La salle se courbe dans un sens,
> repasse exactement droite à zéro, puis se courbe dans l'autre.

### `VibH2O|Mouvement` — comment les bulles bougent

`Float Amplitude` et `Float Speed` règlent le flottement permanent — elles sont sous l'eau, elles
flottent. `Drift Threshold` et `Drift Intensity` règlent la dérive : au-delà du seuil d'excitation,
une bulle s'écarte de sa place. **Elle ne peut mathématiquement pas atteindre la case voisine**,
`Drift Max Ratio` s'en charge.

### `VibH2O|Bulles` — l'aspect

`Base Scale`, `Scale Per Excitation` et `Beat Pop Amount` règlent la taille et le sursaut au
battement. `Organic Deform` et `Organic Speed` règlent la respiration de la silhouette : chaque
bulle se déforme sur trois axes à des cadences légèrement différentes, avec une phase qui lui est
propre — aucune n'est jamais une sphère.

**`Show Seat Numbers`** écrit le numéro de chaque personne au-dessus de sa bulle. C'est ainsi qu'on
vérifie un plan de salle d'un coup d'œil.

La couleur suit l'excitation sur **trois** paliers — bleu profond au repos, sarcelle en transition,
ambre à pleine excitation. Pour la voir travailler, mettre le simulateur en scénario
*Vague d'excitation*.

### `VibH2O|Tableaux` — passer au vortex

**`Blend Alpha`** est le curseur central de l'œuvre : **0** = le plan de salle, **1** = le vortex.
Faites-le glisser en Simulate et regardez les bulles quitter leur siège pour s'enrouler en cône.

Vous pouvez vous arrêter à mi-chemin, revenir en arrière : la transition est continue dans les deux
sens, et les cœurs ne perdent jamais le rythme.

Le tableau 2 est un **vrai banc de poissons** : séparation, alignement et cohésion évalués sur le
voisinage réel, plus un attracteur qui promène le banc. Chaque bulle regarde là où elle nage. Ses
réglages sont dans le pilote `Flock Driver`, catégorie `Banc`.

**La synchronie collective en règle l'ordre** : à 1 le banc est serré et rapide, à 0 il se disperse
en nuage lâche. Pour le voir respirer, scénario *Balayage de synchronie* avec `Blend Alpha` à 1.

---

## 3. Piloter le simulateur

Sélectionner **`VibH2O_Simulateur`** dans l'Outliner.

### `VibH2O|Simulation|Salle`

`Columns` et `Rows` donnent les dimensions — **modifiables pendant que ça tourne** : la salle se
redimensionne sous le curseur. `Aisle Columns` vide des colonnes entières — les allées centrales.

### `VibH2O|Simulation|Donnees`

`Scenario` change ce que fait le public simulé :

| Scénario | Ce que vous verrez |
|---|---|
| **Nominal** | Salle calme, chacun à son rythme. |
| **Vague d'excitation** | Une vague traverse la salle de gauche à droite. |
| **Balayage de synchronie** | La synchronie collective monte et redescend. **À combiner avec `Blend Alpha` à 1** : le vortex se resserre puis se disperse. |
| **Pic collectif** | Toute la salle monte ensemble, puis retombe. |
| **Coupure de capteurs** | Des capteurs se taisent : les bulles concernées se désaturent et cessent de battre. |

`Send Rate Hz` règle la cadence d'envoi, `Base Bpm Min`/`Max` la plage de rythmes cardiaques.

### Par la console

Ouvrir la console avec la touche **²** (ou `~`), puis :

| Commande | Effet |
|---|---|
| `VibH2O.Simulate` | Démarre ou arrête la simulation. |
| `VibH2O.Simulate ales` | Charge **la vraie salle d'Alès** : 23 × 4 avec ses deux allées. |
| `VibH2O.Simulate 22x8` | Une salle de 176 places, l'échelle réelle du spectacle. |
| `VibH2O.Simulate off` | Arrête. |
| `VibH2O.ShowDebug` | Affiche l'état : réseau, salle, moyennes. **Le premier réflexe quand rien n'apparaît.** |
| `VibH2O.DumpRoom` | Écrit le plan de salle dans le log, une ligne par rangée. |

---

## 4. Brancher le vrai Max

Rien à changer. Le simulateur **s'efface tout seul** dès que de vrais paquets arrivent sur le
réseau, et reprend après trois secondes de silence. La même carte sert donc à la démo, au réglage
et à la répétition.

1. Vérifier le port dans **Edit → Project Settings → Plugins → VibH2O**, catégorie *Reseau*.
   Défaut : **9002**, sur toutes les interfaces.
2. Côté Max, régler l'IP du PC Windows et le même port.
3. Lancer, puis `VibH2O.ShowDebug` : si les paquets arrivent, les compteurs montent.

### Si rien n'apparaît

`VibH2O.ShowDebug` dit lequel des trois problèmes vous avez :

- **Écoute INACTIVE** → le port est déjà pris par un autre logiciel.
- **0 paquet** → rien n'arrive : pare-feu, mauvaise IP, ou Max n'émet pas.
- **Paquets reçus mais aucune salle** → le plan de salle n'a pas encore été envoyé depuis Max.

### Pour tester sans Max, avec du vrai réseau

```bash
python Tools/vibh2o_osc_sim.py --preset ales --scenario wave
```

Ce simulateur-là passe par un vrai socket UDP — utile pour valider le réseau lui-même, pare-feu
compris.

---

## 5. Donner leur apparence aux bulles

Le plugin **ne décide pas du look**. Il pousse des valeurs dans le matériau et émet des événements ;
le reste vous appartient.

### Créer sa bulle

1. Clic droit dans le Content Browser → **Blueprint Class** → chercher **`VibH2OBubbleActor`**.
2. Y mettre son mesh et son matériau.
3. Sur `VibH2O_Stage`, catégorie `VibH2O|Bulles`, renseigner **`Bubble Class`** avec ce Blueprint.

> **Un piège à connaître.** L'acteur de scène écrit à chaque frame la position, la rotation et
> **l'échelle de la racine** de la bulle. Mettez votre mesh à l'échelle **sur son propre composant**,
> pas sur la racine — sinon votre réglage est écrasé. C'est voulu : la place et la taille d'une
> bulle appartiennent à la salle, pas à la bulle.

### Les valeurs disponibles dans le matériau

Créez des paramètres portant exactement ces noms ; le plugin les remplit tout seul.

| Paramètre | Contenu |
|---|---|
| `BeatPhase` | 0→1, **continu, ne saute jamais**. La base de tout ce qui est périodique. |
| `BeatPulse` | Retombe de 1 à 0 après chaque battement. |
| `Bpm` | Le rythme cardiaque brut. |
| `Excitation` | 0→1. |
| `Synchrony` | 0→1, pour cette personne. |
| `CollectiveSynchrony` | 0→1, pour toute la salle. |
| `StriationSpeed` | Vitesse réelle de la bulle. |
| `FocusMask` | 1 dans le groupe en focus, moins en dehors. |
| `Staleness` | 1 si le capteur s'est tu. |
| `BlendAlpha` | Position entre les deux tableaux. |
| `TintColor` | Couleur (paramètre vectoriel). |

**L'astuce des anneaux concentriques**, dans le matériau :

```
frac(BeatPhase * 6 - Distance * 10)
```

produit un train d'anneaux qui s'étendent en permanence, calés sur le vrai cœur. Ils ne
glitcheront jamais lors d'un changement de rythme — c'est toute la raison pour laquelle
`BeatPhase` est continu.

Le matériau `M_VibH2ODemoBubble` livré avec le plugin montre les onze paramètres en action.
Regardez-le, il n'est pas long.

### Les événements Blueprint

Dans votre Blueprint de bulle, clic droit → chercher :

- **`Sur battement`** — à chaque battement de cette personne. Pour déclencher un son, un Niagara,
  des particules.
- **`Sur mise a jour des donnees`** — chaque frame, avec l'état complet.
- **`Sur affectation a un siege`** — quand la bulle reçoit sa place.

---

## 6. Isoler une partie de la salle — le focus

Sur `VibH2O_Stage`, catégorie `VibH2O|Focus` :

1. Ajouter une entrée à **`Focus Groups`**, lui donner un nom, et définir son rectangle en colonnes
   et rangées. `Extra Seat Indices` permet d'ajouter des sièges isolés.
2. Renseigner **`Active Focus Group`** avec ce nom.
3. Monter **`Focus Amount`** de 0 à 1.

Les bulles hors du groupe s'assombrissent progressivement. **Rien ne se déplace** : le plan de salle
reste lisible.

Pour cadrer une caméra sur le groupe, le plugin donne la cible sans piloter la caméra —
`Get Focus Center`, `Get Focus Bounds` et `Get Focus Fit Distance` sont appelables en Blueprint.

---

## 7. Animer dans Sequencer

Tous les réglages pilotables sont animables. Dans une Level Sequence, ajouter l'acteur `VibH2O_Stage`,
puis **+ Track** → les propriétés apparaissent : `Blend Alpha`, `Curvature Angle`, `Focus Amount`,
et tout le reste.

La séquence d'exemple **`Content/Sequences/LS_VibH2O_Recette`** anime les trois principales sur dix
secondes.

> **Pendant une séquence, Sequencer l'emporte sur l'OSC** — automatiquement, parce que la piste
> réécrit la valeur à chaque frame alors que l'OSC n'écrit qu'à la réception. Pour couper l'entrée
> OSC franchement pendant un rendu, passer `bIgnore Osc Control` à vrai sur le subsystem.

---

## 8. Vérifier que tout va bien

| Besoin | Commande |
|---|---|
| L'outillage réseau est sain | `python Tools/selftest_osc.py` |
| Le plugin est correct | `Automation RunTests VibH2O` dans la console |
| Voir la recette visuelle complète | `VibH2O.DemoSweep` — quatorze captures dans `Saved/VibH2OSweep/`, puis le moteur quitte |

---

## Aide-mémoire

| Je veux… | Faire… |
|---|---|
| Voir la salle vivre | Ouvrir `VibH2O_Demo`, **Alt + S** |
| Régler la géométrie | Sélectionner `VibH2O_Stage`, catégorie `VibH2O|Geometrie` |
| Passer au vortex | `Blend Alpha` de 0 à 1 |
| Changer de salle | `VibH2O.Simulate ales` |
| Comprendre pourquoi rien n'apparaît | `VibH2O.ShowDebug` |
| Voir le plan en texte | `VibH2O.DumpRoom` |
| Changer le port réseau | Project Settings → Plugins → VibH2O → *Reseau* |
| Mettre mon propre look | Blueprint dérivé de `VibH2OBubbleActor`, puis `Bubble Class` |
