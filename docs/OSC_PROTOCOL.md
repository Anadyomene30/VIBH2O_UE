# Protocole OSC

Tout ce qui circule entre le patch **Max/MSP** (émetteur, sur Mac) et le **plugin Unreal**
(récepteur, sur Windows). Transport : **OSC sur UDP**, IP et port réglables (défaut `9002`).

La source de vérité côté émetteur est le patch `VibH2O_RoomMapping.maxpat` du dépôt `VibH2o`.

---

## 1. Le plan de salle — `/RoomMapping`

**Confirmé par capture réelle.** Le plan arrive à plat, un message OSC par siège.

```
/RoomMapping/columns/ 5
/RoomMapping/rows/    5
/RoomMapping/1/       1
/RoomMapping/2/       6
/RoomMapping/3/       11
...
/RoomMapping/25/      25
```

- `columns` et `rows` donnent les dimensions de la grille.
- `/RoomMapping/<index de siège>/` porte en argument l'**identifiant de l'individu** assis là.
- L'index de siège commence à **1**.

### ⚠️ Piège 1 — le slash final

Les adresses se terminent par un slash : `/RoomMapping/columns/`, et non `/RoomMapping/columns`.

Un découpage naïf de l'adresse produit donc un **dernier segment vide**. Le parseur doit ignorer
les segments vides plutôt que de rejeter le message.

### ⚠️ Piège 2 — une rafale sans marqueur de fin

Un plan de salle 5 × 5 arrive sous forme de **27 messages consécutifs** (2 dimensions + 25
sièges), sans aucun message signalant la fin de l'envoi.

Reconstruire la salle à chaque message la reconstruirait **27 fois de suite**, avec autant de
créations et destructions d'acteurs.

**Solution :** accumuler les messages dans un tampon, et ne construire qu'après **~100 ms de
silence** sur les adresses `/RoomMapping`. Le délai doit être réglable.

### ⚠️ Piège 3 — l'identifiant 0 signifie « siège vide »

Un `0` en argument ne désigne pas un individu : il signale un **siège inoccupé**.

Conséquences, toutes obligatoires :

- **Aucune bulle** n'est créée à cette place — la grille a des trous, c'est normal.
- L'individu 0 n'existe pas : aucune donnée ne lui est routée.
- Il est **exclu du calcul de la synchronie collective**. L'oublier ferait compter un siège vide
  comme une personne à zéro de synchronie, ce qui fausserait tout le tableau 2.
- Il est **exclu des bornes de cadrage du focus**, sinon la caméra viserait du vide.
- Un siège peut **passer à 0 en cours de route** : la bulle doit alors disparaître.

> **Une valeur de plus à surveiller.** `Scripts/FormatRoomMapping.js` remplace une cellule vide
> par **999** sur l'un de ses chemins (`value === 0 ? 999 : value - 3`), et soustrait 3 aux autres.
> Impossible de dire, sur pièces seules, lequel de ces chemins alimente l'envoi OSC réellement
> capturé — la capture de référence, elle, montre bien des `0`.
>
> Le plugin traite donc **0 et 999** comme « siège vide » (`EmptySeatIds`), et expose un
> `IndividualIdOffset` laissé à 0. Si les identifiants apparaissent décalés de 3 en salle, c'est ce
> champ qu'il faut ajuster — pas le code.

> **Un index absent n'est pas un siège vide.** Le protocole annonce toujours explicitement une
> place libre. Un index manquant est du silence — le plus souvent un datagramme UDP perdu. Le
> traiter comme un effacement ferait disparaître une bulle à chaque paquet perdu. Le réglage
> `bTreatMissingSeatsAsEmpty` est donc **faux** par défaut.

### ⚠️ Piège 4 — l'ordre de parcours est ambigu

Dans la capture de référence, les individus 1 à 5 apparaissent aux index **1, 6, 11, 16, 21** :

| index OSC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | … |
|---|---|---|---|---|---|---|---|---|
| identifiant | 1 | 6 | 11 | 16 | 21 | 2 | 7 | … |

Or l'interface de Max affiche les sièges 1 à 5 sur la **première rangée**, en lecture
horizontale. Il y a donc une **transposition** entre l'index OSC et la lecture visuelle : soit
l'index parcourt la salle en colonnes, soit les identifiants sont numérotés en colonnes.

**Ce piège est invisible sur une grille carrée.** En 5 × 5, une salle transposée ressemble à une
salle correcte — l'erreur ne se révélerait qu'en salle, avec les mauvaises personnes aux mauvaises
places.

**Solution :** exposer un réglage `SeatOrder` (`RowMajor` / `ColumnMajor`) basculable sans
recompilation, et **valider sur une salle non carrée — 7 colonnes × 3 rangées**. C'est le seul
test qui lève le doute. Une fois la bonne valeur connue, la noter dans ce fichier.

> **Valeur correcte constatée :** **`ColumnMajor`** — l'index OSC descend une colonne, puis passe
> à la suivante.
>
> Établie **sur pièces, pas par déduction**, dans le dépôt `VibH2o` :
>
> 1. `Vib-e.motion/Scripts/FormatRoomMapping.js` — le script qui fabrique les messages — calcule
>    `index = (x * rows + y) + 1`.
> 2. Dans un `matrixctrl` Max, la coordonnée est `x y` = **colonne, rangée**. Le preset réel
>    `RoomMapping_Presets/ALES_FINAL.json` le confirme sans ambiguïté : pour `columns: 23,
>    rows: 4`, ses clés vont de `"0 0"` à `"22 3"` — donc `x` va jusqu'à 22, c'est bien la colonne.
> 3. Recoupement : avec cette formule et une numérotation visuelle en rangées, une salle 5 × 5
>    place les individus 1 à 5 aux index 1, 6, 11, 16, 21 — **exactement la capture ci-dessus**.
>
> `Tools/selftest_osc.py` rejoue ce recoupement à chaque exécution.
>
> **Le test 7 × 3 en salle reste à faire malgré tout** : un patch Max peut être modifié sans que ce
> code le soit, et seul le test in situ prouve que le patch tournant ce soir-là est bien celui-ci.
> Le réglage `SeatOrder` est là pour cette raison.

---

## 2. Les données live

Format suivant le style de `/RoomMapping` : une adresse par individu et par mesure.

```
/BPM/<id>/          72.3     rythme cardiaque, en battements par minute
/SD/<id>/           0.42     excitation, DÉJÀ NORMALISÉE par Max
/Synchronie/<id>/   0.81     synchronie de l'individu avec le groupe
```

> **Relevé sur les patchs.** Dans `VIBH2O_Mapping.maxpat`, les préfixes sont des **textedit**
> initialisés à `/BPM` et à **`/Synchronie`** — et non `/Sync` comme le supposait ce document. Le
> réglage `SynchronyPrefix` du plugin vaut donc `/Synchronie` par défaut.
>
> `/SD` **reste à confirmer** : aucun textedit correspondant n'a été retrouvé dans les patchs
> consultés. Si l'adresse diffère, seul le champ `ExcitationPrefix` est à changer.
>
> Le port `9002` est confirmé deux fois : par le textedit du patch `VibH2O_RoomMapping` et par
> l'objet `udpsend 192.168.0.255 9002` de `VIB.e-motion.maxpat`.

Les préfixes sont des **réglages de projet, pas des constantes** — *Project Settings → Plugins →
VibH2O*. Si le patch les nomme autrement, c'est un champ à modifier, pas du code à recompiler.

### Ce que Max envoie et n'envoie pas

- **Max n'envoie pas d'événement par battement**, seulement une valeur de BPM. Le rythme est donc
  reconstruit côté Unreal par accumulateur de phase — voir `CLAUDE.md`.
- **La baseline est calculée dans Max.** La valeur d'excitation reçue est déjà un écart normalisé
  par rapport au repos de la personne. Unreal ne calcule aucune baseline.
- La **synchronie collective** n'est pas transmise : Unreal la calcule comme la moyenne des
  synchronies individuelles, **en excluant les sièges vides et les capteurs muets**.

### Capteurs muets

Aucun message ne signale qu'un capteur a décroché. Le plugin horodate chaque mise à jour et, au
delà d'un délai réglable (défaut **5 s**), bascule l'individu en état « muet » : la bulle se
désature et cesse de battre, plutôt que d'afficher indéfiniment une valeur périmée. Un individu
muet sort des moyennes.

---

## 3. Les messages de contrôle

Reçus par le plugin pour piloter le rendu depuis Max — ce qui couvre le **pilotage MIDI**, un
contrôleur physique entrant dans Max et ressortant en OSC.

```
/Effect/<Nom>/ 0|1        active ou désactive un effet sur toute la salle
/Blend/        0.0–1.0    morph du tableau 1 vers le tableau 2
/Focus/Group/  <nom>      sélectionne un groupe de focus défini dans Unreal
/Focus/Amount/ 0.0–1.0    intensité du focus
```

Ces mêmes commandes sont accessibles depuis le panneau Details, depuis Blueprint, et animables
dans Sequencer.

**Arbitrage OSC / Sequencer :** l'OSC écrit à la réception d'un message, Sequencer réécrit à
chaque frame pendant la lecture — Sequencer l'emporte donc naturellement pendant une séquence. Un
réglage `bIgnoreOscControl` permet de couper franchement l'entrée OSC pendant un rendu, pour
éviter qu'un message reçu en cours de route ne vienne polluer la piste.

---

## 4. Exigences du parseur

OSC 1.0. Le parseur doit :

- Décoder les **messages** et les **bundles** (`#bundle`).
- Gérer les types `i` (entier 32 bits), `f` (flottant 32 bits), `s` (chaîne).
- Respecter le **padding à 4 octets** des chaînes et des adresses.
- **Tolérer le slash final** et les segments vides.
- **Ne jamais crasher** sur une trame tronquée, mal formée ou inattendue — en représentation, un
  paquet perdu ne doit pas emporter le spectacle.
- Rester **sans dépendance au moteur**, pour être testable hors d'Unreal.

---

## 5. Vérification

Le dépôt `VibH2o` contient déjà `Scripts/OSC_SIMULATOR.py`, qui simule **176 capteurs** en OSC.
**Le regarder avant d'en écrire un autre** — l'étendre est probablement préférable.

Le simulateur doit permettre de couvrir :

| Cas | Ce qu'il vérifie |
|---|---|
| Salle 5 × 5 | Le fonctionnement nominal |
| Salle 7 × 3 | **La transposition** — le test indispensable |
| Salle d'Alès, 23 × 4 | Le vrai plan, avec ses deux allées : non carré **et** troué |
| Sièges à 0 | Les trous dans la grille et l'exclusion des moyennes |
| ~176 sièges | La tenue en charge à l'échelle réelle |
| Vague d'excitation | Les effets continus |
| Coupure d'un individu | Le passage en état muet |
| Balayage de synchronie | Le comportement du vortex |
