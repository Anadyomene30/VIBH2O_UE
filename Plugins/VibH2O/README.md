# Plugin VibH2O

Reçoit en OSC/UDP les données cardiaques de spectateurs et les donne à voir sous forme de bulles
disposées comme le plan de la salle.

- Le **pourquoi** des décisions : [`../../CLAUDE.md`](../../CLAUDE.md)
- Le **protocole réseau** : [`../../docs/OSC_PROTOCOL.md`](../../docs/OSC_PROTOCOL.md)
- La **liste des fonctions** : [`../../docs/SPEC.md`](../../docs/SPEC.md)

---

## Installation

Le plugin est autonome : il ne dépend d'aucun autre plugin, pas même de l'OSC d'Epic.

1. Copier `Plugins/VibH2O/` dans le dossier `Plugins/` du projet d'accueil.
2. Le projet doit être un **projet C++**. S'il est Blueprint-only, ajouter n'importe quelle classe
   C++ suffit à créer les fichiers de build.
3. Régénérer les fichiers de projet, puis compiler.
4. Le plugin apparaît activé dans *Edit → Plugins → Installation*.

**Compilé et testé sous Unreal 5.5 et Unreal 5.8**, à partir du même code source, sur les cibles
Editor, Game et Shipping — les quatorze tests d'automation passent sous les deux moteurs. Aucune
`EngineVersion` n'est figée dans le `.uplugin`, et aucune API instable n'est utilisée.

> Le plugin OSC d'Epic n'est **pas** employé : son statut et son API bougent d'une version à
> l'autre, ce qui rendrait impossible l'objectif d'un seul code source. Le socket UDP et le parseur
> OSC 1.0 sont écrits ici, sans dépendance.

---

## Démarrage rapide

```bash
python Tools/vibh2o_osc_sim.py --preset 7x3
```

Puis, dans le niveau :

1. Poser un **`VibH2O — Acteur de scène`** (`AVibH2OStageActor`).
2. Lancer en PIE. Les bulles apparaissent.
3. Console : `VibH2O.ShowDebug` pour l'état réseau, `VibH2O.DumpRoom` pour le plan en texte.

Si rien n'apparaît, `VibH2O.ShowDebug` dit pourquoi : port occupé, aucun paquet, plan absent.

---

## Les classes

| Classe | Rôle |
|---|---|
| `UVibH2OSettings` | Réglages de projet : port, adresses OSC, délais, ordre des sièges. |
| `UVibH2OSubsystem` | Le modèle vivant : réseau, plan de salle, état par individu, moyennes. Ne connaît aucun acteur. |
| `AVibH2OStageActor` | La racine de l'installation. **Possède les bulles**, en permanence. |
| `AVibH2OBubbleActor` | Une bulle. À dériver en Blueprint pour lui donner son apparence. |
| `UVibH2OPositionDriver` | Un tableau : une fonction qui dit où va chaque bulle. |
| `UVibH2OGridDriver` | Tableau 1 — le plan de salle. |
| `UVibH2OVortexDriver` | Tableau 2 — le vortex, amorce. |
| `FVibH2OOscParser` | Parseur OSC 1.0, sans dépendance au moteur. |
| `FVibH2OLayoutMath` | Toute la géométrie, en fonctions pures. Testée hors acteur. |

**Les bulles n'appartiennent à aucun tableau.** C'est la décision structurante du projet : les deux
pilotes tournent en permanence et `BlendAlpha` interpole entre eux. La transition est donc continue,
réversible, arrêtable à mi-chemin, et la phase de battement ne se perd jamais.

---

## Le contrat matériau

Le plugin crée une instance dynamique sur le premier composant de mesh trouvé — ou sur celui
désigné par `TargetMeshComponent` — et y pousse ces valeurs à chaque frame.

### Les neuf du contrat

| Paramètre | Type | Contenu |
|---|---|---|
| `BeatPhase` | scalaire | 0→1, **continu, ne saute jamais**. Le socle de tout ce qui est périodique. |
| `BeatPulse` | scalaire | Enveloppe décroissante depuis le dernier battement. |
| `Bpm` | scalaire | Valeur brute. |
| `Excitation` | scalaire | 0→1, normalisée par Max. |
| `Synchrony` | scalaire | 0→1, synchronie de cette personne. |
| `StriationSpeed` | scalaire | Vitesse mesurée de la bulle × `StriationSpeedScale`. |
| `FocusMask` | scalaire | 1 dans le groupe en focus, moins en dehors. |
| `Staleness` | scalaire | 1 si le capteur est muet. |
| `TintColor` | vecteur | Couleur issue du gradient d'excitation. |

### Au-delà du contrat

`CollectiveSynchrony` et `BlendAlpha` sont poussés en plus, parce que le matériau du tableau 2 en a
besoin et qu'aucun autre chemin ne serait moins coûteux. **Vider le nom dans `ParameterNames`
suffit à ne pas les envoyer** — comme pour n'importe lequel des neuf.

### Un point à connaître avant de dessiner la bulle

L'acteur de scène **écrit chaque frame** la position, la rotation et l'échelle **relatives de la
racine** de la bulle. Une échelle posée sur le composant racine dans le Blueprint sera donc écrasée.

C'est voulu : la place, l'orientation et la taille d'une bulle appartiennent à la salle, pas à la
bulle. Pour donner une forme propre au mesh, **le mettre à l'échelle sur son propre composant**,
sous la racine — pas sur la racine elle-même.

### Les anneaux concentriques

Ils n'ont besoin d'aucun état :

```
frac(BeatPhase * N - Distance * K)
```

produit un train d'anneaux qui s'étendent en permanence, calés sur le cœur réel. C'est la
continuité de `BeatPhase` qui garantit qu'ils ne glitchent jamais lors d'un changement de BPM — et
c'est pourquoi la phase est un accumulateur et non un timer.

### Deux pièges de rendu

- **Viser un matériau opaque ou masked en subsurface.** Un matériau translucide ne reçoit
  correctement ni les ombres ni les caustiques dans Unreal ; l'environnement étant sous-marin, la
  transparence doit être feinte par du Fresnel.
- **L'émissif module l'éclairage, il ne le remplace pas.**

---

## Les événements Blueprint

Sur `AVibH2OBubbleActor`, à surcharger dans la sous-classe Blueprint :

| Événement | Quand |
|---|---|
| `Sur battement` | À chaque battement de cette personne. Pour le son, Niagara, les particules. |
| `Sur mise à jour des données` | Chaque frame, après le calcul de position. |
| `Sur affectation à un siège` | À la création, et si le siège change d'occupant. |

Sur `UVibH2OSubsystem` : `OnRoomChanged`, `OnBeat`, `OnEffectChanged`, et les trois entrées de
contrôle OSC.

Sur `AVibH2OStageActor` : `OnFocusGroupChanged`, `OnRoomBuilt`.

---

## Sequencer

Tout paramètre pilotable porte le spécificateur `Interp`, ce qui le rend visible dans la liste des
propriétés animables. Les plus utiles : `BlendAlpha`, `FocusAmount`, `CurvatureAngle`,
`FloatAmplitude`, `DriftIntensity`, et les dix réglages du vortex.

**Arbitrage OSC / Sequencer.** L'OSC n'écrit qu'à la réception d'un message ; Sequencer réécrit la
propriété à chaque frame pendant la lecture. Sequencer l'emporte donc naturellement. Pour couper
franchement l'entrée pendant un rendu, mettre `bIgnoreOscControl` du subsystem à vrai : les données
des capteurs continuent d'arriver, seul le **contrôle** (`/Effect`, `/Blend`, `/Focus`) est ignoré.

---

## Le focus

Un groupe est une **zone rectangulaire** en colonnes et rangées, plus une liste de **sièges isolés**
pour les sélections éparses qu'un rectangle ne sait pas décrire.

```
GetFocusBounds()                      -> FBox, en monde
GetFocusCenter()                      -> FVector
GetFocusFitDistance(HorizontalFOV)    -> float
```

Le plugin **ne pilote pas la caméra** : il expose la cible de cadrage. La mise en scène reste au
metteur en scène — Cine Camera, Blueprint ou Level Sequence.

Les bornes sont calculées depuis les **positions réellement occupées par les bulles**, jamais depuis
une grille plate supposée. C'est ce qui les rend justes sur une salle courbée et après déplacement
de l'acteur de scène — les deux cas où une implémentation naïve échoue.

---

## Les effets

Portée **globale**, pour toute la salle. Six sont câblés en dur :

`Float`, `Drift`, `Beat`, `Tint`, `Focus`, `Scale`

Tout autre nom est accepté, stocké et relayé par `OnEffectChanged` — un artiste peut donc en définir
sans toucher au C++. Un effet jamais nommé est considéré **actif** : une faute de frappe dans un nom
n'éteint rien en silence.

```
/Effect/Drift/ 0        depuis Max
```

---

## Prévisualisation dans l'éditeur

`bPreviewInEditor` dessine la grille dans le viewport **sans instancier la moindre bulle** : le
niveau reste propre. C'est ce qui permet de balayer `CurvatureAngle` et de vérifier
`bOrientToCenter` sans lancer le jeu ni le simulateur.

`bPreviewShowOrientation` ajoute une flèche par siège — indispensable, car sur des sphères une
rotation ne se voit pas.

---

## Commandes console

| Commande | Effet |
|---|---|
| `VibH2O.ShowDebug` | Bascule l'affichage réseau / salle / moyennes. |
| `VibH2O.DumpRoom` | Écrit le plan dans le log, une ligne par rangée. **L'outil du test 7 × 3.** |
| `VibH2O.Restart` | Redémarre l'écoute avec les réglages courants. |

---

## Une convention de langue

Le `CLAUDE.md` demande **le code et les commentaires en anglais**, la documentation et les échanges
en français. La frontière retenue ici :

- **Ce que lit un développeur** — commentaires, noms de classes et de propriétés — est en anglais.
- **Ce que lit l'opérateur en salle** — messages de log, affichage de débogage, libellés
  d'assertion des tests, noms affichés dans le panneau Details — reste en français.

---

## Ce qui reste hors périmètre

- **Le comportement de banc** du tableau 2 : voisinage, séparation, alignement, cohésion. Le vortex
  livré donne la forme, ce qui suffit à rendre la transition testable.
- **Le look définitif des bulles** : mesh, matériau subsurface, stries, anneaux. La sphère de
  démonstration prouve que les données circulent, rien de plus.
