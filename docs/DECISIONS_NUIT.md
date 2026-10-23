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
