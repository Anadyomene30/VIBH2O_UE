# Attracteurs, zones d'exclusion et télécommande iPad

Pendant le spectacle, on veut **appeler** une partie des bulles vers un endroit choisi, puis les
**renvoyer** à leur place, et **protéger** les deux circassiennes qui évoluent en l'air devant le
tulle : aucune bulle ne doit passer devant elles. Le tout se pilote depuis un iPad, dans le noir.

Trois pièces :

| Pièce | Ce que c'est |
|---|---|
| **Attracteur** (`VibH2O — Attracteur`) | Une sphère placée dans le niveau. Activé, il appelle sa part de la salle ; les bulles s'y rassemblent en nuage, qui respire et continue de battre. Désactivé, elles reviennent à leur place. |
| **Zone d'exclusion** (`VibH2O — Zone d'exclusion`) | Une ellipse ou un rectangle arrondi, vu depuis la caméra. Les bulles en sont écartées, sur toute la profondeur de la scène. On la fait glisser en direct pour suivre une circassienne. |
| **Télécommande** | Une page web servie par le plugin lui-même, installée sur l'iPad comme une appli plein écran. |

---

## 1. Comment ça marche — en une minute

La pile de position de chaque bulle devient :

```
Lerp(Grille, Banc, BlendAlpha)   ← les tableaux, inchangés
  → attracteurs                  ← nouveau : tire vers les nuages
  → flottement + dérive          ← toujours actifs
  → zones d'exclusion            ← nouveau : en dernier, rien ne peut ramener une bulle dedans
```

Deux conséquences, voulues :

- **Le retour est exact, sans mémoire.** Les pilotes de grille et de banc ne s'arrêtent jamais : un
  attracteur ne fait que tirer *par-dessus*. Quand il lâche, la bulle retombe sur une place qui lui a
  été gardée tout du long — y compris si le banc a bougé entre-temps.
- **Tout est fluide, même un contre-ordre.** Chaque bulle suit sa cible par un ressort critique :
  départ sans à-coup, arrivée sans rebond, et si l'on change d'avis à mi-course, elle ralentit et fait
  demi-tour au lieu de sauter. Les durées sont étalées d'une bulle à l'autre (`Etalement`), si bien
  que la salle part comme un flux, pas comme un bloc.

Une bulle dont le trajet traverse une zone d'exclusion **la contourne** : elle glisse le long du bord
jusqu'à l'autre côté, à la vitesse `Vitesse de contournement`, au lieu de sauter d'un bord à l'autre
— un saut qui, sinon, se produirait pile devant la circassienne.

---

## 2. Placer un attracteur

1. **Place Actors** → chercher `VibH2O — Attracteur` → le déposer dans le niveau.
2. **Le nommer** dans l'outliner (`Haut gauche`, `Tissu`…). Le *Nom de contrôle* se remplit tout
   seul d'après le nom de l'acteur : c'est lui qui apparaît sur l'iPad et dans l'OSC. Un nom tapé à la
   main n'est plus jamais écrasé ; vider le champ le rend au nom de l'acteur.
3. **Le dimensionner** : la sphère est le volume du nuage d'arrivée. Une échelle non uniforme donne un
   nuage aplati ou étiré.
4. **L'attacher à l'acteur de scène** (le glisser sur `VibH2O_Stage` dans l'outliner) s'il doit
   suivre l'installation quand on la déplace. Sinon il reste fixe dans le monde. Les deux marchent :
   tout se calcule chaque image dans l'espace local de la scène.
5. Régler, dans le panneau Details :

| Catégorie | Réglage | Rôle |
|---|---|---|
| `VibH2O\|Pilotage` | **Activation** | Cible de 0 à 1. L'iPad envoie 0 ou 1 ; une valeur intermédiaire gare les bulles à mi-chemin. |
| | Nom affiché, Couleur du bouton, Ordre d'affichage | Ce que montre l'iPad. Une couleur par attracteur aide dans le noir. |
| | Suivre l'OSC et l'iPad | À décocher pour un attracteur piloté seulement par Sequencer. |
| `VibH2O\|Selection` | **Part des bulles** | 0,3 = 30 % de la salle. Toujours les mêmes bulles, tirées de leur siège : la scène se rejoue à l'identique chaque soir. |
| | **Début de la part** | Où commence la part sur le cercle de sélection. Deux attracteurs à 0 et 0,3, chacun de part 0,3, appellent **deux groupes disjoints** ; à 0 et 0, les mêmes bulles. |
| `VibH2O\|Fondu` | Durée d'arrivée / de retour | 4 s et 6 s par défaut — le temps pour couvrir 99 % du chemin. |
| | **Etalement** | 0 : tout le groupe d'un bloc. 0,6 : les premières partent nettement avant les dernières. |
| `VibH2O\|Nuage` | Respiration, Vitesse de respiration | Le nuage enfle et se resserre doucement. |

**Essayer sans iPad** : le bouton **Basculer** du panneau Details active ou désactive l'attracteur,
dans l'éditeur (viewport en temps réel, `Ctrl + R`) comme en Play.

Plusieurs attracteurs peuvent être actifs ensemble. Si deux parts se recouvrent, la bulle concernée se
place entre les deux nuages, de façon stable.

---

## 3. Placer une zone d'exclusion

1. **Place Actors** → `VibH2O — Zone d'exclusion` → la poser **là où la circassienne apparaît à
   l'image**, au centre du tulle de préférence (c'est l'origine du déplacement en direct).
2. **L'orienter** : sa **flèche** (axe X) doit pointer **dans le sens où regarde la caméra**, vers le
   tulle. La boîte se lit alors comme la caméra la voit : son étendue **Y** est la demi-largeur, **Z**
   la demi-hauteur. (Une flèche tournée vers la caméra inverse gauche et droite sur l'iPad : cocher
   *Inverser gauche / droite* ou retourner la zone.)
3. **La dimensionner** : `Box Extent` Y et Z, ou l'échelle de l'acteur. Par défaut 1,8 m × 2,6 m.
   Compter le rayon d'une bulle en plus, ou l'inclure dans la *Marge douce*.
4. **Désigner la caméra de projection** (*Camera de projection*) — recommandé. La zone devient alors un
   vrai cône depuis l'objectif : elle couvre la circassienne **à toutes les profondeurs**, avec la
   perspective. Sans caméra, la zone pousse droit selon sa flèche, ce qui suffit pour une caméra
   lointaine.
5. **Ne pas l'attacher** à l'acteur de scène, en général : la circassienne est liée au tulle et à la
   caméra, pas à l'installation.

| Catégorie | Réglage | Rôle |
|---|---|---|
| `VibH2O\|Pilotage` | **Activation** | 1 par défaut : une zone posée protège dès qu'elle est là. Fondu de 1,5 s. |
| `VibH2O\|Forme` | Forme | Ellipse, ou rectangle aux coins arrondis (aucun coin où une bulle accrocherait). |
| | **Marge douce** | Bande autour de la forme où les bulles sont ralenties vers le bord au lieu de s'y cogner. |
| | Vitesse de contournement | En degrés par seconde ; 150 : une bulle prise au centre fait le tour en un peu plus d'une seconde. |
| | Limiter en profondeur | Décoché : toute la profondeur de la scène (ce qui tombe sur la circassienne à l'image, quelle que soit la distance). Coché : seulement dans l'étendue X de la boîte. |
| `VibH2O\|Deplacement` | **Position X / Y** | De −1 à 1 ; c'est ce que l'iPad fait glisser. Animable dans Sequencer. |
| | **Amplitude** | Demi-largeur et demi-hauteur du déplacement, en cm. 400 × 300 = un tulle de 8 m × 6 m. |
| | Lissage | La zone suit le doigt avec ce léger retard : aucune saccade, même par un Wi-Fi irrégulier. |

Dans l'éditeur, faire varier *Position X / Y* déplace la boîte immédiatement : c'est ainsi qu'on vérifie
que l'amplitude couvre bien le tulle vu de la caméra.

---

## 4. Connecter l'iPad

La télécommande est **servie par Unreal lui-même**, sur le port **8642** (*Project Settings → Plugins →
VibH2O → Telecommande iPad*). Rien d'autre à lancer, rien à installer sur le PC, rien qui expire.

1. **Même réseau.** L'iPad et le PC Unreal sur le même Wi-Fi (ou le PC en filaire sur le routeur du
   Wi-Fi).
2. **Lancer le jeu** (Play, Standalone ou la version packagée). Le log Unreal écrit l'adresse :

   ```
   VibH2O: telecommande iPad — ouvrir http://192.168.1.12:8642/ dans Safari.
   ```

   Elle apparaît aussi dans l'affichage de débogage (`VibH2O.ShowDebug`), ligne *iPad*.
3. **Pare-feu Windows** : au premier lancement, Windows demande s'il faut autoriser Unreal (ou
   l'exécutable du jeu) sur le réseau. **Autoriser sur les réseaux privés.** Si la question a été
   refusée par le passé : *Pare-feu Windows → Autoriser une application* → cocher *UnrealEditor* (et
   l'exécutable packagé). C'est la panne n° 1 d'une installation.
4. **Sur l'iPad** : Safari → l'adresse ci-dessus → bouton **Partager** → **Sur l'écran d'accueil**.
   L'icône (une bulle bleue) ouvre désormais la télécommande en plein écran, sans barre Safari.
5. **Le point vert** en haut à gauche dit qu'Unreal répond. Rouge : plus de réponse — Wi-Fi, pare-feu,
   ou jeu arrêté.

L'adresse du PC peut changer d'un lieu à l'autre : lui donner une **IP fixe** (ou une réservation DHCP
sur le routeur) évite d'avoir à reconfigurer l'iPad à chaque salle.

> **Le PC a plusieurs cartes réseau ?** Le log écrit une adresse par carte. Prendre celle du réseau
> où est l'iPad.

> **Port déjà pris ?** Le log le dit : `Impossible d'ouvrir la telecommande sur …`. Changer le port
> dans les Project Settings. Piège vu sur la machine de développement : Logitech G HUB occupe 9080 sur
> 127.0.0.1 — Windows laisse alors un second programme écouter le même port sur 0.0.0.0, sans erreur.
> Le plugin sonde ce cas au démarrage et l'écrit dans le log.

### Dans le noir

| Bouton | Effet |
|---|---|
| ☾ | Assombrit tout l'écran (en plus de la luminosité de l'iPad). Mémorisé. |
| 🔒 | Bloque toutes les touches, contre un appui involontaire. Un second appui déverrouille. |
| ? | L'aide, avec l'adresse du PC. |

La page demande à l'iPad de garder l'écran allumé, mais selon la version d'iPadOS une appli posée sur
l'écran d'accueil n'y a pas droit : régler *Réglages → Luminosité → Verrouillage automatique* sur
**Jamais** pour la représentation. Pour empêcher de quitter l'appli par erreur : *Réglages →
Accessibilité → Accès guidé*, puis triple-clic.

### Ce que montre l'iPad

- **Attracteurs** — une tuile par attracteur, à sa couleur. Un appui appelle, un second renvoie. La
  barre montre où en est le fondu (*en route · 62 %*, *arrivé*, *retour*, *au repos*).
- **TOUT RELÂCHER** — renvoie tout le monde, chacun à son propre fondu. La sortie de secours.
- **Durée des fondus** — multiplie toutes les durées, de ×0,25 à ×4 ; cran doux à ×1.
- **Le tulle** — les zones à leur position et à leur taille ; on les fait glisser du doigt. Les
  pastilles dessous les allument et les éteignent.
- **Second onglet** — le morph *Salle → Banc*, le bouton **Fond** (`/Fond 1|0` : le fond sous-marin
  part en fondu vers le noir, les bulles restent), le groupe et l'intensité du focus, les effets.

### Modifier la page sur place

Elle tient dans un fichier : `Plugins/VibH2O/Resources/Remote/index.html`. Dans une version
packagée, ce dossier est copié **à côté de l'exécutable, hors du pak** : on peut le modifier avec un
éditeur de texte, taper `VibH2O.Restart` dans la console Unreal, puis recharger la page. Le réglage
*Dossier de la page* permet aussi d'en servir une autre sans toucher au plugin.

---

## 5. Les trois chemins de commande

Comme tout le reste du plugin, chaque commande existe dans le panneau Details, en Blueprint et en
OSC. L'iPad n'est qu'un émetteur de plus : il envoie les **mêmes messages** que Max ou TouchOSC
pourraient envoyer, et ils suivent le même chemin dans le plugin.

| Adresse OSC | Argument | Effet |
|---|---|---|
| `/Attractor/<nom>/` | 0 à 1 (sans argument : 1) | Activation de l'attracteur |
| `/Attractor/<nom>/Share/` | 0 à 1 | Sa part de la salle |
| `/Attractors/Release/` | — | Tous les attracteurs lâchent |
| `/Attractors/FadeScale/` | 0,1 à 10 | Multiplicateur de toutes les durées |
| `/Exclusion/<nom>/` | 0 ou 1 | Activation de la zone |
| `/Exclusion/<nom>/Position/` | x y, de −1 à 1 | Position en direct |

Ces messages arrivent aussi sur le port OSC habituel (`9002`) : Max peut donc déclencher un attracteur
sur un événement du patch. `bIgnoreOscControl` les coupe tous, iPad compris — l'iPad l'affiche alors en
bandeau.

**Sequencer** : `Activation`, `Part des bulles`, les durées, `Position X / Y` et `Multiplicateur des
fondus` portent `Interp`. Pour qu'un attracteur ne soit commandé que par la piste, décocher *Suivre
l'OSC et l'iPad*. `TOUT RELÂCHER` épargne ces attracteurs-là.

**Blueprint** : `SetActive`, `Toggle`, `GetCurrentWeight` sur l'attracteur ; `SetDragPosition` sur la
zone ; `ReleaseAllAttractors`, `GetAttractors`, `GetExclusionZones` sur la scène.

---

## 6. Vérifier

Automatisé, dans la suite habituelle (`Automation RunTests VibH2O`) :

| Test | Ce qu'il mesure |
|---|---|
| `VibH2O.Steering.FonduActivation` | Départ en douceur, aucune secousse, 99 % à la durée réglée, retour exact à 0 ; contre-ordre à mi-course sans saut ni rebond ; même fondu à 30 et à 144 images/s ; étalement ; cumul de plusieurs attracteurs continu. |
| `VibH2O.Steering.PartDeLaSalle` | Une part de 30 % appelle 30 % de la salle ; deux parts adjacentes ne se recouvrent pas et couvrent tout ; le nuage remplit son volume sans en sortir. |
| `VibH2O.Steering.ProjectionHorsZone` | Aucune bulle laissée dans la zone, centre compris ; rien ne bouge au-delà de la marge ; une traversée par le centre contourne sans saut — et un **témoin** sans mémoire du côté prouve que le test sait voir le saut. |
| `VibH2O.Remote.Telecommande` | Lecture des commandes et des en-têtes HTTP ; un vrai aller-retour TCP : commandes en file, état servi, aucun fichier hors de la page. |

De bout en bout, contre le jeu qui tourne — le script joue le rôle de l'iPad :

```bash
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/dimit/Documents/GitHub/VIBH2O_UE/VIBH2O_UE.uproject" -game -nullrhi -unattended -nosplash -NoSound
```

```bash
python Tools/vibh2o_remote_check.py
```

Il appelle un attracteur, mesure la montée (mi-chemin vers 0,8 s, aucun recul, aucun saut), change
d'avis à mi-course, vérifie le retour exact à 0, puis déplace, éteint et rallume une zone.

> **Depuis Git Bash**, ne pas passer la carte en argument (`/Game/Maps/…`) : MSYS la convertit en
> chemin Windows et le jeu s'arrête aussitôt. La carte de démo est la carte par défaut, inutile de la
> nommer.

La carte de démonstration (`Tools/make_demo_map.py`) contient deux attracteurs au-dessus de la salle,
*Haut gauche* et *Haut droite*, chacun appelant son propre tiers du public, et une zone
*Circassienne* liée à la caméra.

---

## 7. Dépannage

| Symptôme | Cause probable |
|---|---|
| Point rouge, page qui ne charge pas | Pas le même réseau ; pare-feu Windows ; jeu arrêté ; mauvaise IP (le PC a changé d'adresse). |
| La page charge, mais *Aucun attracteur dans le niveau* | La carte ouverte n'en contient pas, ou le jeu tourne sur une autre carte. |
| Un appui ne fait rien | Attracteur réglé pour ne pas *Suivre l'OSC et l'iPad* (la tuile le dit : *Sequencer*) ; ou bandeau *Contrôle coupé* : `bIgnoreOscControl`. |
| Les bulles passent quand même devant la circassienne | Zone éteinte, trop petite (ajouter le rayon des bulles), ou flèche mal orientée ; désigner la *Camera de projection*. |
| Gauche et droite inversés sur le tulle | Flèche de la zone tournée vers la caméra : cocher *Inverser gauche / droite*. |
| L'attracteur appelle « personne » | *Part des bulles* à 0, ou salle sans bulles. |
| `attracteur 'X' inconnu` dans le log | Le nom envoyé ne correspond à aucun *Nom de contrôle* (la casse ne compte pas). |
