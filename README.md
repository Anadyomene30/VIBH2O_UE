# VIBH2O_UE

Dépôt de **documentation et de passation** du projet VIBH2O.

## 👉 Commencer ici

**[PASSATION_CLAUDE_CODE.md](PASSATION_CLAUDE_CODE.md)** — tout ce qu'il faut faire, dans
l'ordre, pour installer Claude Code sur un nouvel ordinateur, récupérer le code, installer
les dépendances et reprendre le travail sur le projet.

## Les dépôts du projet

| Dépôt | Visibilité | Contenu |
|---|---|---|
| [`Anadyomene30/VibH2o`](https://github.com/Anadyomene30/VibH2o) | **privé** | Cœur du projet : patchs Max/MSP, scripts JS et Python, ressources |
| [`eva-decorps/VibH2OServer`](https://github.com/eva-decorps/VibH2OServer) | public | Serveur Node/Express : dashboard spectateurs, QR codes par siège |
| [`Anadyomene30/VIBH2O_UE`](https://github.com/Anadyomene30/VIBH2O_UE) | public | Ce dépôt — documentation et passation |
| [`Anadyomene30/VIBH2O_REBORN`](https://github.com/Anadyomene30/VIBH2O_REBORN) | public | Réécriture Unreal Engine — **vide à ce jour** |

## Le projet en deux lignes

VIBH2O capte en temps réel les données cardiaques (BPM / intervalles, HRV) des spectateurs
assis dans une salle, les transmet en **OSC** vers un patch **Max/MSP** qui pilote le
spectacle, et les restitue aux spectateurs via un **dashboard web** accessible par QR code
depuis leur siège.

## Contacts

- Dimitri Sourzac — `dimitri.sourzac@gmail.com` (projet, patchs Max)
- Eva Decorps — serveur web
