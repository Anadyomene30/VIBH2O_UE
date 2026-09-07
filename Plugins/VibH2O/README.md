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

**Sans rien brancher.** Ouvrir la carte `Maps/VibH2O_Demo` et appuyer sur Play : le
**simulateur intégré** (`VibH2O — Simulateur`, posé dans le niveau) joue le rôle de Max — plan de
salle, BPM, excitation, synchronie — et la salle vit immédiatement.

Dans un autre niveau, deux gestes suffisent :

1. Poser un **`VibH2O — Acteur de scène`** (`AVibH2OStageActor`).
2. Poser un **`VibH2O — Simulateur`** (`AVibH2OSimulatorActor`) — ou taper `VibH2O.Simulate`
   dans la console, qui en crée un au besoin. `VibH2O.Simulate ales` charge la vraie salle
   d'Alès, `22x8` une salle de 176, `off` l'arrête.

Ses scénarios se règlent dans le panneau Details : nominal, vague d'excitation, balayage de
synchronie (pour voir respirer le vortex avec `BlendAlpha`), pic collectif, coupure de capteurs.

**Le jour où le vrai Max émet, il n'y a rien à faire** : dès que des paquets arrivent sur le
socket, le simulateur se met en pause tout seul, et reprend après trois secondes de silence
réseau. La même carte sert donc à la démo et à la répétition. Et comme il injecte ses messages
par le même chemin que le réseau — adresses construites depuis les réglages du projet, rafale,
silence, transposition — ce qu'on juge à l'œil est le vrai comportement du plugin.

**Avec le vrai flux réseau :**

```bash
python Tools/vibh2o_osc_sim.py --preset 7x3
```

Console : `VibH2O.ShowDebug` pour l'état réseau, `VibH2O.DumpRoom` pour le plan en texte.
Si rien n'apparaît, `VibH2O.ShowDebug` dit pourquoi : port occupé, aucun paquet, plan absent.

---

## Les classes

| Classe | Rôle |
|---|---|
| `UVibH2OSettings` | Réglages de projet : port, adresses OSC, délais, ordre des sièges. |
| `UVibH2OSubsystem` | Le modèle vivant : réseau, plan de salle, état par individu, moyennes. Ne connaît aucun acteur. |
| `AVibH2OStageActor` | La racine de l'installation. **Possède les bulles**, en permanence. |
| `AVibH2OBubbleActor` | Une bulle. À dériver en Blueprint pour lui donner son apparence. |
| `AVibH2OSimulatorActor` | Le Max de poche : simule plan et données par le chemin d'injection, et s'efface dès que le vrai réseau parle. |
| `UVibH2OFlockDriver` | Tableau 2 — vrais boids : séparation, alignement, cohésion, attracteur mouvant. Pilote par défaut. |
| `AVibH2OWaterMotes` | Les particules en suspension dans l'eau. |
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
niveau reste propre. `bPreviewAnimate` y ajoute le flottement et un battement de convenance, pour
que la salle respire pendant qu'on règle la géométrie — l'éditeur n'a aucune donnée, ce battement
n'est donc qu'un artifice de lisibilité.

**Mesuré, pas supposé :** hors Play, `StageTime` avance en continu dans le monde éditeur —
`VibH2O.PreviewProbe` le vérifie en une commande. C'est ce qui permet de balayer `CurvatureAngle`
et de vérifier
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
| `VibH2O.PreviewProbe` | Mesure si l'acteur de scène tourne dans l'éditeur, hors Play. Absente des builds Shipping. |
| `VibH2O.Simulate [ales\|CxR\|off]` | Simule le patch Max sans rien brancher. Sans argument : bascule. Crée un simulateur si le niveau n'en a pas. |
| `VibH2O.DemoSweep` | Déroule la recette visuelle complète — une capture PNG par critère dans `Saved/VibH2OSweep/` — puis quitte. Absente des builds Shipping. |

---

## La passe artistique de la carte de démonstration

Ce que la carte montre, et où le régler :

| Effet | Où |
|---|---|
| **Silhouette organique** | `VibH2O_Stage` → `Organic Deform` / `Organic Speed`. Trois axes, trois cadences, une phase par bulle : aucune n'est une sphère, aucune ne se déforme comme sa voisine, et l'excitation accentue l'irrégularité. |
| **Couleur selon l'excitation** | Rampe à **trois** paliers dans `M_VibH2ODemoBubble` — bleu profond, sarcelle, ambre. Un dégradé à deux couleurs passerait par un gris mort au milieu, là où se tient la majorité du public. |
| **Numéros sur les bulles** | `VibH2O_Stage` → `Show Seat Numbers`. Tournés vers la caméra à chaque frame. |
| **Banc de poissons** | `VibH2O_Stage` → `Flock Driver`, catégorie `Banc`. La synchronie collective serre ou disperse le banc. |
| **Particules dans l'eau** | Acteur `VibH2O_Particules`. Le champ entier dérive comme un corps, pas mote par mote. |
| **Éclairage** | Brouillard **volumétrique** + soleil diffusant + `PostProcess` (bloom, vignette, étalonnage froid). |

> **Trois limites de l'API Python d'Unreal 5.5, rencontrées et contournées.** Le *world position
> offset* d'un matériau n'est pas atteignable depuis Python : la déformation de silhouette se fait
> donc dans le C++, sur l'échelle de l'acteur. Le nœud `Noise` refuse sa propriété `Function`.
> Et un composant **instancié** rendait toutes ses instances noires avec un matériau qui
> s'affiche correctement sur un composant ordinaire — vérifié par deux témoins côte à côte — d'où
> quelques centaines de particules sur le chemin qui marche plutôt que des milliers sur celui qui
> ne marche pas.

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
- **Le look définitif des bulles** : mesh, matériau subsurface, stries, anneaux. Le matériau de
  démonstration (`/VibH2O/M_VibH2ODemoBubble`, régénérable par `Tools/make_demo_material.py`)
  prouve que les onze paramètres circulent et agissent — il ne prétend à aucune direction
  artistique.
