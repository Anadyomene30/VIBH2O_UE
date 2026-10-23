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
