# Passation — Installer et reprendre le projet VIBH2O avec Claude Code sur un autre ordinateur

> Ce document décrit **tout ce qu'il faut faire, dans l'ordre**, pour qu'une personne
> qui n'a jamais touché au projet puisse, sur une machine neuve, installer Claude Code,
> récupérer le code, installer les dépendances et reprendre le travail.
>
> Durée estimée : **45 min à 1 h 30** (hors installation de Max/MSP).
> Plateforme de référence : **macOS** (le projet est développé sur Mac ; les scripts
> `generate_qr.sh` et `CreateDMG` sont spécifiques macOS). Les équivalents Windows/Linux
> sont indiqués quand ils existent.

---

## Table des matières

1. [Avant de commencer — ce qu'il faut avoir en main](#1-avant-de-commencer--ce-quil-faut-avoir-en-main)
2. [Installer les prérequis système](#2-installer-les-prérequis-système)
3. [Installer Claude Code](#3-installer-claude-code)
4. [Connecter le compte GitHub](#4-connecter-le-compte-github)
5. [Récupérer les dépôts du projet](#5-récupérer-les-dépôts-du-projet)
6. [Installer les dépendances du projet](#6-installer-les-dépendances-du-projet)
7. [Vérifier que tout fonctionne (checklist de recette)](#7-vérifier-que-tout-fonctionne-checklist-de-recette)
8. [Travailler avec Claude Code sur ce projet](#8-travailler-avec-claude-code-sur-ce-projet)
9. [Conventions Git du projet](#9-conventions-git-du-projet)
10. [Ce qui n'est PAS dans Git — à transférer à la main](#10-ce-qui-nest-pas-dans-git--à-transférer-à-la-main)
11. [Dépannage](#11-dépannage)
12. [Checklist finale de passation](#12-checklist-finale-de-passation)

---

## 1. Avant de commencer — ce qu'il faut avoir en main

Rien de ce qui suit ne s'installe sans ces accès. **Les réunir en premier** évite de
rester bloqué au milieu de la procédure.

| Élément | Pourquoi | Où le récupérer |
|---|---|---|
| **Compte Claude** (Pro, Max ou Team) *ou* une **clé API Anthropic** | Authentifier Claude Code | [claude.ai](https://claude.ai) / [console.anthropic.com](https://console.anthropic.com) |
| **Compte GitHub** | Cloner et pousser le code | [github.com](https://github.com) |
| **Accès au dépôt privé `Anadyomene30/VibH2o`** | C'est le cœur du projet, il est **privé** | Demander à Dimitri (`dimitri.sourzac@gmail.com`) d'ajouter le compte comme *collaborator* : `Settings → Collaborators → Add people` |
| **Licence Max/MSP** (Cycling '74) | Ouvrir les patchs `.maxpat` | [cycling74.com](https://cycling74.com) — licence perso ou version d'essai 30 j |
| **Les fichiers hors-Git** | Enregistrements, données de spectacle | Voir [§10](#10-ce-qui-nest-pas-dans-git--à-transférer-à-la-main) |

> ⚠️ **Sans accès au dépôt privé `VibH2o`, la passation est impossible.**
> C'est la première chose à débloquer.

---

## 2. Installer les prérequis système

### macOS

```shell
# 1. Xcode Command Line Tools (fournit git)
xcode-select --install

# 2. Homebrew (si absent) — https://brew.sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 3. Les outils du projet
brew install node        # Node.js 18+ : requis par Claude Code ET par le serveur
brew install git
brew install python      # Python 3 : scripts d'analyse et simulateur OSC
brew install qrencode    # génération des QR codes des sièges
```

### Windows

- **Node.js 18+** : [nodejs.org](https://nodejs.org) (installeur LTS)
- **Git** : [git-scm.com](https://git-scm.com/download/win)
- **Python 3** : [python.org](https://python.org) — cocher *« Add Python to PATH »*
- `qrencode` n'existe pas nativement : utiliser WSL, ou générer les QR codes depuis un Mac.

### Vérifier

```shell
node --version    # doit afficher v18.x ou plus
npm --version
git --version
python3 --version # doit afficher 3.x
```

---

## 3. Installer Claude Code

### 3.1 Installation

**Option A — installeur natif (recommandé, pas de dépendance npm globale)**

```shell
# macOS / Linux
curl -fsSL https://claude.ai/install.sh | bash
```

```powershell
# Windows (PowerShell)
irm https://claude.ai/install.ps1 | iex
```

**Option B — via npm**

```shell
npm install -g @anthropic-ai/claude-code
```

> ❌ **Ne jamais installer avec `sudo npm install -g`** : cela crée des problèmes de
> permissions et casse les mises à jour automatiques.

### 3.2 Premier lancement et connexion

```shell
cd ~/Documents/VIBH2O   # ou le dossier où tu vas cloner les dépôts
claude
```

Au premier lancement, Claude Code propose de se connecter :

- **Abonnement Claude** (Pro/Max/Team) → ouvre le navigateur, se connecter avec le compte Claude.
- **Clé API Anthropic** → coller la clé (facturation à l'usage via la Console).

Si la fenêtre ne s'ouvre pas, taper la commande `/login` dans Claude Code.

### 3.3 Vérifier l'installation

Dans Claude Code :

```
/status      → affiche le compte connecté, le modèle, le dossier de travail
/model       → choisir le modèle (Opus pour les tâches complexes, Sonnet pour le reste)
/help        → liste toutes les commandes disponibles
```

Dans le terminal :

```shell
claude doctor        # diagnostic de l'installation
claude --version
claude update        # mise à jour manuelle
```

### 3.4 (Optionnel) Intégration à l'éditeur

- **VS Code / Cursor** : extension *Claude Code* dans la marketplace.
- **JetBrains** (IntelliJ, PyCharm…) : plugin *Claude Code*.
- **Navigateur / téléphone** : [claude.ai/code](https://claude.ai/code) — sessions distantes
  qui tournent dans le cloud, sans rien installer (voir [§8.4](#84-travailler-depuis-le-web-ou-le-téléphone)).

---

## 4. Connecter le compte GitHub

### 4.1 Identité Git locale

```shell
git config --global user.name  "Prénom Nom"
git config --global user.email "adresse@exemple.com"
```

### 4.2 Authentification pour `git push`

**Option A — HTTPS + Personal Access Token (le plus simple)**

1. GitHub → `Settings → Developer settings → Personal access tokens → Fine-grained tokens`
2. Créer un token avec accès **Contents: Read and write** sur les dépôts VIBH2O.
3. Au premier `git push`, coller le token comme mot de passe.
4. Le mémoriser :
   ```shell
   git config --global credential.helper osxkeychain   # macOS
   git config --global credential.helper manager       # Windows
   ```

**Option B — SSH**

```shell
ssh-keygen -t ed25519 -C "adresse@exemple.com"
pbcopy < ~/.ssh/id_ed25519.pub     # macOS : copie la clé publique
# puis GitHub → Settings → SSH and GPG keys → New SSH key → coller
ssh -T git@github.com              # doit répondre "Hi <user>! You've successfully authenticated"
```

### 4.3 Connecter GitHub à Claude (pour les sessions web)

Nécessaire uniquement si on veut lancer Claude Code depuis [claude.ai/code](https://claude.ai/code) :

1. claude.ai → `Settings → Connectors` → connecter **GitHub**.
2. Autoriser l'accès aux dépôts `Anadyomene30/VibH2o`, `Anadyomene30/VIBH2O_UE`,
   `Anadyomene30/VIBH2O_REBORN`, `eva-decorps/VibH2OServer`.

---

## 5. Récupérer les dépôts du projet

Le projet est éclaté sur **4 dépôts**. Les cloner côte à côte dans un dossier parent :

```shell
mkdir -p ~/Documents/VIBH2O && cd ~/Documents/VIBH2O

# 1. Le cœur du projet : patchs Max, scripts, ressources  (PRIVÉ — accès requis)
git clone https://github.com/Anadyomene30/VibH2o.git

# 2. Le serveur web temps réel (dashboard spectateurs, QR codes)
git clone https://github.com/eva-decorps/VibH2OServer.git

# 3. Le dépôt de documentation / passation (celui-ci)
git clone https://github.com/Anadyomene30/VIBH2O_UE.git

# 4. Réécriture Unreal Engine  (⚠ dépôt VIDE à ce jour)
git clone https://github.com/Anadyomene30/VIBH2O_REBORN.git
```

### État réel des dépôts (constaté le 2026-09-05)

| Dépôt | Visibilité | Contenu | Remarque |
|---|---|---|---|
| `Anadyomene30/VibH2o` | **privé** | 502 fichiers, 1 seul commit | Tout le projet Max/MSP est là |
| `eva-decorps/VibH2OServer` | public | serveur Node/Express + templates + scripts | Le seul avec un `README.md` |
| `Anadyomene30/VIBH2O_UE` | public | ce guide | Était vide avant cette passation |
| `Anadyomene30/VIBH2O_REBORN` | public | **vide** | Aucun commit, aucune branche |

> 📌 Le dépôt `VibH2o` n'a **qu'un seul commit** (`update ecart individu`) : il n'y a
> donc **aucun historique** à consulter. Toute la connaissance du projet est dans le
> code lui-même et dans ce document.

---

## 6. Installer les dépendances du projet

### 6.1 Serveur web — `VibH2OServer`

```shell
cd ~/Documents/VIBH2O/VibH2OServer
npm install            # installe express + chartjs-plugin-annotation
```

Dépendances déclarées dans `package.json` :
- `express` ^5.1.0 — serveur HTTP
- `chartjs-plugin-annotation` ^3.1.0 — annotations (les « drapeaux ») sur les graphiques

Les bibliothèques front (`lib/chart.min.js`, `lib/chartjs-plugin-annotation.min.js`) sont
**déjà versionnées** dans le dépôt : rien à faire de plus.

### 6.2 Scripts Python

```shell
python3 -m pip install python-osc
```

Utilisé par `VibH2o/Vib-e.motion/Scripts/OSC_SIMULATOR.py`, qui simule l'envoi de
**176 capteurs BPM en OSC** sur le réseau local (adresse `/<SENSOR_ID>`, port 9001 par défaut).

`tkinter` est utilisé par certains scripts d'interface : livré avec Python sur macOS/Windows ;
sur Linux, `sudo apt install python3-tk`.

Les autres scripts (`Ressources/PyScripts/*.py`, `scripts/remove_lines.py`,
`scripts/extract_landmarks.py`) n'utilisent que la bibliothèque standard
(`os`, `re`, `sys`, `math`, `collections`, `argparse`, `socket`) — **rien à installer**.

### 6.3 Max/MSP et ses externals

1. Installer **Max 8** (ou la version utilisée par le projet) depuis [cycling74.com](https://cycling74.com).
2. Les externals sont fournis dans le dépôt, sous `VibH2o/Vib-e.motion/Externals/` :
   - `CNMAT-Externals 6/`
   - `Heartbeat/`
3. Dans Max : `Options → File Preferences…` → ajouter le dossier
   `~/Documents/VIBH2O/VibH2o/Vib-e.motion/` **avec l'option *subfolders* activée**,
   pour que les patchs retrouvent leurs abstractions, scripts JS et externals.

**Patchs principaux** (dans `VibH2o/Vib-e.motion/`) :

| Fichier | Rôle |
|---|---|
| `VIB.e-motion.maxpat` | patch principal (le plus gros, ~940 Ko) |
| `VIBH2O_AcquisitionIntervalles.maxpat` | acquisition des intervalles cardiaques |
| `VIBH2O_Mapping.maxpat` | mapping données → sorties |
| `VibH2O_RoomMapping.maxpat` | plan de salle / placement des sièges |
| `VIBH2O_TriggerEvents.maxpat` | déclenchement d'événements |
| `VIBH2O_DetectionEvent.maxpat` | détection d'événements |
| `EventCreator.maxpat` | création d'événements |

**Scripts JS** appelés par les patchs : `Vib-e.motion/Scripts/` (HRV, filtrage capteurs,
gestion des sièges, presets, enregistrement…). Ce sont des scripts `js`/`jsui` Max :
ils ne se lancent **pas** depuis un terminal, ils s'exécutent dans les patchs.

### 6.4 Génération des QR codes

```shell
brew install qrencode    # macOS uniquement
```

---

## 7. Vérifier que tout fonctionne (checklist de recette)

À faire dans l'ordre. Si une étape échoue, voir [§11 Dépannage](#11-dépannage).

### ✅ 7.1 Claude Code répond

```shell
cd ~/Documents/VIBH2O/VibH2o
claude
```
Puis taper : `résume-moi la structure de ce dépôt`. Une réponse cohérente = OK.

### ✅ 7.2 Le serveur démarre

```shell
cd ~/Documents/VIBH2O/VibH2OServer
mkdir -p data
# copier un enregistrement de test dans data/ et le renommer bpm_data.txt
cp ~/Documents/VIBH2O/VibH2o/Ressources/RECORDS/SESSION_ALES.txt data/bpm_data.txt

# (optionnel) nettoyer les lignes de synchronie
#   → éditer scripts/remove_lines.py pour pointer sur le bon fichier, puis :
python3 scripts/remove_lines.py

node server.js
```

Ouvrir <http://localhost:3000> → le dashboard doit s'afficher.
Un spectateur donné : <http://localhost:3000/user/user1>.

### ✅ 7.3 Les QR codes se génèrent

```shell
cd ~/Documents/VIBH2O/VibH2OServer
chmod +x scripts/generate_qr.sh
./scripts/generate_qr.sh 10 qr_output
```
→ produit `qr_output/user1_qr.png` … `user10_qr.png` + `auth_qr.png`.

> Le script détecte l'IP locale via `ipconfig getifaddr en0` (**Wi-Fi macOS**).
> Sur une machine branchée en Ethernet, remplacer `en0` par la bonne interface
> (`ifconfig` pour la lister).

### ✅ 7.4 Le flux OSC arrive dans Max

```shell
cd ~/Documents/VIBH2O/VibH2o/Vib-e.motion/Scripts
python3 OSC_SIMULATOR.py                       # broadcast auto-détecté
python3 OSC_SIMULATOR.py --host 192.168.1.42   # ou vers une IP précise
python3 OSC_SIMULATOR.py --host 192.168.1.255 --port 9001
```
Ouvrir `VIBH2O_AcquisitionIntervalles.maxpat` dans Max : les valeurs BPM doivent bouger.

### ✅ 7.5 Le patch principal s'ouvre sans erreur

Ouvrir `VIB.e-motion.maxpat`. **Aucun objet ne doit apparaître en pointillés**
(objet manquant = chemin de recherche mal configuré, voir §6.3).

---

## 8. Travailler avec Claude Code sur ce projet

### 8.1 Lancer une session

```shell
cd ~/Documents/VIBH2O/VibH2o     # TOUJOURS se placer dans le dépôt concerné
claude                            # nouvelle session
claude --continue                 # reprendre la dernière session de ce dossier
claude --resume                   # choisir parmi les sessions précédentes
claude -p "question courte"       # réponse ponctuelle, sans mode interactif
```

### 8.2 Le fichier `CLAUDE.md`

Claude Code lit automatiquement le fichier `CLAUDE.md` à la racine du dépôt : c'est là
qu'on met le contexte projet (architecture, conventions, commandes utiles) pour ne pas
avoir à le réexpliquer à chaque session.

- Un `CLAUDE.md` est fourni à la racine de ce dépôt — **le recopier dans `VibH2o`**
  et le compléter au fil de l'eau.
- Pour en générer un automatiquement dans un dépôt : commande `/init`.
- Contexte personnel valable pour tous les projets : `~/.claude/CLAUDE.md`.

### 8.3 Commandes utiles au quotidien

| Commande | Effet |
|---|---|
| `/init` | génère un `CLAUDE.md` à partir du code existant |
| `/clear` | vide le contexte (à faire entre deux tâches sans rapport) |
| `/model` | changer de modèle |
| `/status` | compte, modèle, dossier, connecteurs |
| `/config` | réglages (thème, modèle par défaut…) |
| `/permissions` | gérer ce que Claude peut faire sans redemander |
| `/review` | revue de code des modifications en cours |
| `Shift+Tab` | bascule entre *plan mode* (réfléchit sans modifier) et modes d'édition |
| `Échap` | interrompre Claude en cours de route |
| `Échap` ×2 | remonter dans l'historique des messages |

### 8.4 Travailler depuis le web ou le téléphone

[claude.ai/code](https://claude.ai/code) permet de lancer des sessions Claude Code **dans le cloud**,
sans rien installer : le dépôt est cloné dans un conteneur éphémère, Claude travaille, puis
pousse sur une branche. Pratique pour dépanner en tournée depuis un téléphone.
Prérequis : le connecteur GitHub configuré ([§4.3](#43-connecter-github-à-claude-pour-les-sessions-web)).

> ⚠️ Le conteneur est **éphémère** : tout ce qui n'est pas commité **et poussé** est perdu.

### 8.5 Réglages et permissions

| Fichier | Portée |
|---|---|
| `~/.claude/settings.json` | tous les projets, cette machine |
| `<dépôt>/.claude/settings.json` | ce dépôt, **versionné et partagé** avec l'équipe |
| `<dépôt>/.claude/settings.local.json` | ce dépôt, **local**, à mettre dans `.gitignore` |

---

## 9. Conventions Git du projet

### Branches

- Ne **jamais** travailler directement sur la branche par défaut.
- Branches créées par Claude Code : `claude/<description-courte>-<suffixe>`.
- Branches humaines : `feat/…`, `fix/…`, `docs/…`.

```shell
git checkout -b feat/ma-fonctionnalite
git add -A
git commit -m "Description claire de ce qui change"
git push -u origin feat/ma-fonctionnalite
```

### Ce qu'il ne faut **pas** commiter

Déjà couvert par les `.gitignore` — vérifier avant chaque commit :

- `.DS_Store` (macOS) — il en traîne déjà plusieurs dans `VibH2o`, à nettoyer un jour
- `node_modules/`, `package-lock.json` (côté serveur)
- `data/`, `user/`, `qr_output/` (données de spectacle et QR codes générés)
- Tout enregistrement de spectateur : **données physiologiques = données personnelles**

> 🔒 **RGPD** — les fichiers de `Ressources/RECORDS/` contiennent des données cardiaques
> nominatives par siège. Ne pas les publier dans un dépôt public, ne pas les envoyer à un
> service tiers sans base légale.

---

## 10. Ce qui n'est PAS dans Git — à transférer à la main

À copier depuis l'ancienne machine (clé USB, disque chiffré, transfert direct) :

| Élément | Emplacement typique | Pourquoi hors Git |
|---|---|---|
| Enregistrements de spectacle bruts | `VibH2OServer/data/` | ignoré par `.gitignore`, données personnelles |
| QR codes générés | `VibH2OServer/qr_output/` | régénérables via `generate_qr.sh` |
| `node_modules/` | `VibH2OServer/` | régénérable via `npm install` |
| Licence et préférences Max/MSP | `~/Documents/Max 8/` | licence nominative |
| Presets de salle spécifiques à un lieu | `Vib-e.motion/RoomMapping_Presets/` | vérifier ce qui est versionné |
| Fichiers de config de spectacle | `Vib-e.motion/*.json` (`CONFIGURATION_ALES_1110.json`, `Config_Ales_FINAL.json`, `CIRCLE.json`…) | **versionnés** — juste vérifier qu'ils sont à jour |
| Identifiants réseau du lieu (IP, ports, Wi-Fi) | — | jamais dans Git |

> ✍️ **À compléter par Dimitri** : cette liste est déduite du code et des `.gitignore`.
> Ajouter ici tout ce qui manque (matériel capteurs, adresses IP de la régie, contacts…).

---

## 11. Dépannage

### Claude Code

| Symptôme | Solution |
|---|---|
| `claude: command not found` | Rouvrir le terminal ; sinon ajouter le dossier d'installation au `PATH`, ou réinstaller (§3.1) |
| Erreurs de permission npm | Ne pas utiliser `sudo` — préférer l'installeur natif (Option A) |
| Déconnecté / erreur d'authentification | `/login` dans Claude Code |
| Claude ne voit pas les bons fichiers | Vérifier avec `/status` qu'on est bien dans le bon dossier ; relancer `claude` depuis la racine du dépôt |
| Réponses hors sujet, contexte pollué | `/clear` entre deux tâches |
| Diagnostic général | `claude doctor` |

### Serveur

| Symptôme | Solution |
|---|---|
| `Error: listen EADDRINUSE :::3000` | Un serveur tourne déjà : `lsof -i :3000` puis `kill <PID>` |
| Dashboard vide | Vérifier que `data/bpm_data.txt` existe et n'est pas vide |
| Inaccessible depuis un téléphone | Même réseau Wi-Fi ? Pare-feu macOS ? Utiliser l'IP de la machine, pas `localhost` |
| `generate_qr.sh` : « Impossible de détecter l'adresse IP » | Interface réseau ≠ `en0` : éditer le script (`ipconfig getifaddr en1`…) |

### Max/MSP

| Symptôme | Solution |
|---|---|
| Objets en pointillés dans le patch | Chemin de recherche : ajouter `Vib-e.motion/` avec *subfolders* (§6.3) |
| Aucune donnée capteur | Vérifier le port OSC (9001 par défaut) et le pare-feu ; tester avec `OSC_SIMULATOR.py` |
| Scripts JS sans effet | Vérifier que le dossier `Scripts/` est bien dans le chemin de recherche Max |

### Git / GitHub

| Symptôme | Solution |
|---|---|
| `remote: Repository not found` sur `VibH2o` | Le compte n'est pas *collaborator* du dépôt privé → demander l'accès |
| `Authentication failed` au push | Token expiré → en regénérer un (§4.2) |
| Push refusé (fichier trop gros) | Ne pas commiter les enregistrements et médias lourds (§10) |

---

## 12. Checklist finale de passation

À cocher une par une avant de considérer la passation terminée :

- [ ] Accès **collaborator** au dépôt privé `Anadyomene30/VibH2o` accordé et vérifié
- [ ] Compte Claude (ou clé API) disponible sur la nouvelle machine
- [ ] Node.js 18+, Git, Python 3, `qrencode` installés
- [ ] Claude Code installé — `claude doctor` sans erreur
- [ ] `/login` effectué, `/status` affiche le bon compte
- [ ] Identité Git configurée (`user.name`, `user.email`)
- [ ] Authentification GitHub fonctionnelle (`git push` de test réussi)
- [ ] Connecteur GitHub configuré sur claude.ai (si sessions web souhaitées)
- [ ] Les 4 dépôts clonés côte à côte
- [ ] `npm install` passé dans `VibH2OServer`
- [ ] `python-osc` installé
- [ ] Max/MSP installé + chemin de recherche configuré
- [ ] **Recette §7 complète** : Claude répond, serveur démarre, QR codes générés, OSC reçu dans Max, patch principal sans objet manquant
- [ ] Fichiers hors-Git transférés (§10)
- [ ] `CLAUDE.md` copié dans `VibH2o` et complété
- [ ] Session de passation en direct avec Dimitri (1 h) pour la partie *métier* : déroulé d'un spectacle, calibration capteurs, presets de salle

---

## Contacts

| Rôle | Contact |
|---|---|
| Projet / patchs Max | Dimitri Sourzac — `dimitri.sourzac@gmail.com` |
| Serveur web | Eva Decorps — dépôt `eva-decorps/VibH2OServer` |

---

## Ressources

- Documentation Claude Code : <https://code.claude.com/docs>
- Claude Code sur le web : <https://claude.ai/code>
- Console Anthropic (clés API) : <https://console.anthropic.com>
- Max/MSP : <https://cycling74.com>

---

*Dernière mise à jour : 2026-09-05.*
*Les versions d'outils évoluent : en cas de doute sur une commande Claude Code,
`claude --help` et la doc officielle font foi.*
