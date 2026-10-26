# Tulles en profondeur

> En réserve : rien n'est encore décidé. Le rig existe, il est éteint par défaut, et tant qu'il
> est éteint il ne change rien au spectacle.

## L'idée

La salle a plusieurs tulles étagés : le **tulle face**, que la caméra du rail remplit, et
2,45 m derrière lui les **tulles cour et jardin**. Le **bac** est une bande au sol, devant. Chaque
tulle devient une fenêtre sur le même monde virtuel, et chacun reçoit sa propre image par Spout.

Une bulle assez profonde **et** tombant dans le cadre d'un tulle du fond, vue depuis la caméra,
s'affiche sur ce tulle et disparaît du tulle face. Toutes les autres restent sur le face. Le relief
est donc **réel pour toutes les places** : les images pendent vraiment à des distances différentes.

Le plan coté de la salle est dans [`salle/plan_tulles.svg`](salle/plan_tulles.svg).

## Ce qui est juste, et ce qui ne l'a pas besoin

- Le monde virtuel garde sa taille : le plan de salle reste symbolique, cadré par la caméra.
- Seuls les **rapports entre tulles** sont réels. Le rig les tient en mètres, mesurés en salle,
  par rapport au pied du tulle face, en son milieu.
- L'échelle vient de la caméra. Le tulle face est la coupe du champ de la caméra à une distance
  choisie, et un mètre réel vaut sa largeur divisée par 8. Le reste suit.

## Mettre en place

1. Placer **VibH2O — Tulles en profondeur** dans la carte, n'importe où : sa position ne compte
   pas.
2. Dans l'éditeur, il dessine les tulles devant la caméra du rail. Le face est en jaune, jardin en
   vert, cour en rose ; un tulle inutilisé est en gris.
3. Régler **Distance du tulle face** :
   - **0** : le tulle face est posé sur le plan de la salle, et rien ne part au fond ;
   - plus court : le tulle face remonte vers la caméra, et la salle passe derrière lui.
4. Cocher **Actif**. En jeu, chaque tulle utilisé envoie son image par Spout, sous le nom de la
   liste (*VibH2O Tulle jardin*, *VibH2O Tulle cour*). Le flux principal de la régie ne change pas.

Les tulles se règlent dans **Autres tulles**. **Tulles de la salle mesurée** remet les valeurs du
scan du 4 septembre 2026.

| Réglage | Sens |
|---|---|
| Largeur, hauteur (m) | Celles de l'image projetée. Au sol, la hauteur est la profondeur de la bande. |
| Décalage latéral (m) | Du milieu du tulle face au milieu de celui-ci. Positif vers cour, négatif vers jardin. |
| Profondeur (m) | Distance derrière le tulle face. Négatif : devant. |
| Résolution | Celle de la mire du tulle. |
| Bulles : à partir de / jusqu'à (m) | La tranche de profondeur que ce tulle prend. 1,2 m par défaut, à mi-chemin entre le face et le fond. |
| Hystérésis (m) | Une bulle garde son tulle tant qu'elle reste à moins de cette marge du bord de la tranche : pas de clignotement. |
| Tulles du fond : fond noir | Sans eau ni atmosphère sur les tulles du fond : ce qui n'est pas éclairé reste noir, donc transparent. |

Le **Diagnostic** donne trois choses :
- l'échelle ;
- la distance au tulle face du **spectateur de référence**, la place d'où l'image est
  géométriquement exacte ;
- le nombre de bulles sur chaque tulle.

## À juger, à décider

- **Les mesures.** Les tulles cour et jardin sont estimés à 2,7 × 5,4 m, en recoupant le scan et
  le rapport 1:2 de la mire. En salle, mesurer l'image projetée de chaque mire et la distance entre
  les tulles, puis corriger la liste.
- **Le passage d'un tulle à l'autre** est franc. Une bulle qui franchit la tranche saute de tulle,
  sans fondu. À juger à l'œil ; un fondu sur une bande de profondeur serait l'étape suivante.
- **L'eau.** Le face montre l'eau bleue, les tulles du fond un fond noir. Le face sera donc un voile
  bleu devant eux.
- **L'exposition.** Les captures suivent le volume d'exposition de la caméra. Avec une exposition
  automatique, une image presque toute noire s'éclaircirait : garder l'exposition manuelle.
- **Le reflet.** Un projecteur qui vise les tulles du fond depuis la salle traverse le tulle face et
  y laisse un reflet.
- **Le coût.** Chaque tulle utilisé est un rendu de plus par image. À mesurer avec la salle pleine.
- **Côtés.** Dans le scan, jardin est du côté des rideaux rouges des gradins. À confirmer d'un coup
  d'œil, au cas où le scan serait en miroir.

## Sous le capot

- Le rig suit la caméra active en jeu, et dans l'éditeur la caméra suspendue au premier rail.
- Chaque tulle du fond a une capture dont le frustum décentré passe par ses quatre bords, vu du même
  œil que la caméra principale.
- Les bulles qu'il prend sont retirées de la vue principale par la liste des acteurs cachés du
  joueur, et rendues seules sur leur tulle.
- SpoutUE est facultatif : l'émetteur est créé par son nom. Sans le plugin, les images restent
  disponibles en Blueprint par `GetTulleTarget`.
- La régie laisse ces émetteurs tranquilles : elle ne règle que le flux principal.
- Tests : `VibH2O.Tulles.Geometrie`, `VibH2O.Tulles.Repartition`, `VibH2O.Tulles.SceneEtCamera`.
