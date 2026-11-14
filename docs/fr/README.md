# The Legend of Zelda: Tears of the Kingdom — Carte d’exploration d’Hyrule

[English](../../README.md) · [简体中文](../zh-Hans/README.md) · [日本語](../ja/README.md) · [Español](../es/README.md) · [Русский](../ru/README.md) · **Français** · [Deutsch](../de/README.md)

Ce mod ajoute une couche à la carte du jeu **1.4.3** dans MacNativeGames sur les Mac à puce Apple. Les marqueurs suivent le ciel, la surface et les profondeurs, ainsi que les déplacements et le zoom. Les sanctuaires et les racines activés gardent leurs icônes et la téléportation du jeu.

- **4 428 points dans le paquet de base**, dont les **1 000 noix korogus** : 800 énigmes à une noix et 100 transports à deux noix. Le paquet indique le départ et l’arrivée de chaque transport.
- Retrouvez 152 sanctuaires, 120 racines, 15 tours de reconnaissance et 30 distributeurs d’artéfacts soneaus, ainsi que des grottes, puits, coffres, armures, larmes du dragon et boss.
- Les points suivis disparaissent une fois terminés. Les marqueurs des boss et Lynels qui peuvent réapparaître s’assombrissent après leur défaite puis retrouvent leur éclat à leur retour. Désactivez le filtre pour revoir les points terminés.
- Les huit groupes sont actifs par défaut. La visibilité est enregistrée séparément pour chaque groupe.

## Installation sur Mac

Prévoyez un Mac à puce Apple, Python 3.14, le code source de MacNativeGames, votre jeu de base et sa mise à jour **1.4.3**. Consultez le [guide de compilation de MacNativeGames en anglais](https://github.com/ly05010419/MacNativeGames/blob/main/docs/install.md).

1. À la racine de ce projet, exécutez `python3.14 mac/integrate.py "/path/to/MacNativeGames"`.
2. Compilez ou mettez à jour l’application Mac 1.4.3 depuis ce code. Quittez le jeu avant une mise à jour.
3. Avec la RomFS 1.4.3 extraite, généralement sous `totk/extracted/v143/romfs`, exécutez `python3.14 build_map.py "/path/to/romfs" map-points.json`.
4. Installez le paquet avec `python3.14 mac/configure.py map-points.json --language fr`. Appuyez sur Entrée pour tout garder ou saisissez les numéros à exclure, séparés par des espaces. Ajoutez `--all` pour passer la sélection.

Changez de langue dans la barre de menus du Mac via **View → Carte d’exploration → Langue**. `--language auto` suit la langue du système. Vos catégories sont conservées lors du changement. Le paquet est généré à partir de vos fichiers de jeu et la progression est lue en jeu sans modifier les sauvegardes.

Vous pouvez aussi sélectionner **语言Language：…** en bas du panneau. Les détails des marqueurs s’affichent dans la langue choisie si le paquet contient les traductions.

## Commandes

Ouvrez la carte du jeu avec **Tab / M** ou le bouton **−** de la manette. Changez de niveau avec **3 / 5** ou **↑ / ↓** sur la croix directionnelle.

| Touches | Action |
| --- | --- |
| **⌘⇧L** | Ouvrir ou fermer le panneau des catégories et raccourcis |
| **⌘⇧M** | Afficher ou masquer la couche |
| **⌘⇧P** | Activer ou désactiver le filtre de progression |
| **⌘⌥1–8** | Basculer le groupe et enregistrer sa visibilité |

1. Lieux et exploration · 2. Objets et matériaux · 3. Ennemis et boss · 4. Coffres · 5. Korogus · 6. Équipement · 7. Quêtes · 8. Villages et services

Appuyez brièvement sur **R3** pour ouvrir ou fermer le panneau, ou maintenez-le environ 0,7 seconde pour basculer la couche. Dans le panneau, utilisez la croix haut/bas pour choisir une ligne, gauche/droite pour changer de page et les boutons de validation et de retour du jeu. Les flèches, Entrée, Échap, la souris et la molette fonctionnent aussi.

[Format du paquet en anglais](../map-pack.md) · [Licence MIT](../../LICENSE)
