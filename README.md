# kiln

Moteur 2D en C++17, Win32 + OpenGL 3.3, zéro dépendance. Le jeu qui tourne
dessus s'appelle **Lueur** : une petite flamme qui rallume les foyers d'une
grotte éteinte par le Froid.

Les mondes : la grotte, la forêt, le volcan (Lueur y saute plus haut et la
lave la relance), la mer (elle y est plus faible et l'eau l'éteint) et la
glace, où le froid la ronge : une barre de chaleur baisse loin des feux et
remonte près d'eux. La glace fond sous ses pieds, les murs de glace fondent
quand on s'appuie dessus ou qu'on tire dedans. Et le ciel, sans sol : Lueur y
gagne le pouvoir de planer (maintenir le saut en tombant), des colonnes de vent
la portent, les moulins se lancent d'une flèche et la pluie l'éteint.

Puis le cœur du Froid, un combat : rallumer quatre braseros pour briser son
bouclier, puis le frapper. Le vaincre donne les flèches de feu, qui allument
les feux de loin et font fondre la glace d'un coup.

Ensuite la forge en ruine : leviers à frapper qui ouvrent des portes, pilons,
scies, tapis roulants, métal en fusion qui relance, fourneaux à allumer d'une
flèche de feu pour faire marcher les machines. Au début de la grande forge,
Lueur gagne l'élan (shift ou f, LB à la manette) : une ruée qui brise les
planches pourries et allonge les sauts.

Enfin le monde des ombres, où certains chemins n'existent que dans la lumière.
Lueur y gagne l'éclat (v ou i, B à la manette) : une explosion de lumière qui
dissipe les ombres et révèle les plateformes. Tout au fond, la Nuit : un éclat
la cloue sur place, et il faut tous les pouvoirs pour en venir à bout.

## Build

Il faut g++ (MSYS2 ucrt64) dans le PATH.

```
build.bat            le jeu en debug, avec console
build.bat release    optimisé, sans console, + build\game.pak
build.bat kpack      l'outil qui prépare les assets
build.bat tests      les tests unitaires
build.bat mini       le petit exemple de plateformer (samples/mini.cpp)
build.bat breakout   le vieil exemple
```

Pour faire un autre jeu avec le moteur : [docs/guide.md](docs/guide.md).

Lancer depuis le dossier du projet : `build\lueur.exe`. Tant que `assets\`
est à côté, le jeu prépare les assets lui-même et les recharge dès qu'un
fichier change.

## Commandes

flèches, zqsd ou wasd pour bouger, espace (ou k, c) pour sauter, x (ou j) pour
utiliser l'arme en main, e (ou l) pour changer d'arme. Avec la lance : haut + x
frappe au-dessus, bas + x en l'air frappe en dessous. Avec l'arc : maintenir x
pour viser, haut/bas pour régler l'angle, relâcher pour tirer. Chaque torche ou
brasero allumé redonne des flèches. P ou Tab met en pause, Tab passe un
dialogue, F11 plein écran, F2 capture d'écran. Manette XInput aussi (Y ou RB
pour changer d'arme). Pouvoirs, une fois gagnés : planer (maintenir le saut en
tombant), élan (shift ou f), éclat (v ou i).

## Menus

- **options** : volume de la musique et des effets, plein écran, et les
  touches. Chaque action accepte jusqu'à trois touches ; en choisir une la met
  en premier et la retire des autres actions. Le tout va dans `options.json`,
  à côté de la sauvegarde. Les panneaux du jeu affichent les touches choisies.
- **mondes** : rejouer n'importe quel niveau déjà atteint, avec les pouvoirs
  qu'on avait à ce moment-là. Le meilleur nombre de braises par niveau est gardé
  dans `progress.json`, et rejouer ne touche pas au point de sauvegarde.
- **bonus et reliques** : ramasser toutes les braises d'un monde débloque un bonus
  passif (carquois plus grand, plané plus lent, froid moins mordant, éclat plus
  rapide, élan plus long, lumière plus grande, flèches aux feux, Lueur dorée).
  Cinq reliques cachées dans les autels peuvent être équipées (jusqu'à 2) auprès
  d'un foyer allumé via le menu pause.

## Le moteur

- rendu : sprites par lots, résolution fixe agrandie en entier, cibles de
  rendu, éclairage, parallaxe (`src/parallax.h`)
- ECS à ensembles clairsemés, physique de plateforme, collisions
- audio : mixeur waveOut, synthé pour les bruitages (`.sfx`) et la musique (`.song`)
- animations, tweens, particules
- KScript, un petit langage de script (`src/script.h`)
- scènes et préfabs en JSON, éditeur intégré (F1 en jeu)
- pipeline : sprites en texte (`.pix`) ou en PNG (grille, ou export
  Aseprite avec ses tags), atlas, pak, vérification de toutes les
  références entre fichiers
- fonctions de script communes à tous les jeux, interrupteurs entre scripts
  (`src/script_api.h`)
- touches réassignables par action (`src/actions.h`), caméra qui suit, tremble
  et se tourne vers un boss (`src/camera.h`)
- texte UTF-8, accents compris

## Le jeu

`game/` pour le code, `assets/` pour tout le reste. L'histoire, les dialogues
et les cinématiques sont dans `assets/data/story.json`, les niveaux dans
`assets/scenes/`. Les décors de parallaxe sont générés par `tools/bggen.py`.

## Pour les tests

- `LUEUR_LEVEL=n` démarre au niveau n
- `LUEUR_SPAWN=x,y` fait apparaître le joueur ailleurs
- `LUEUR_EDIT=1` ouvre le niveau dans l'éditeur
- `LUEUR_SKIP_STORY=1` saute dialogues et cinématiques
- `LUEUR_TRACE=1` affiche la position du joueur
- `KILN_INPUT=fichier` rejoue une suite de touches (voir `src/engine.cpp`)
- `KILN_SHOT=fichier.png` prend une capture et quitte

## Contribuer

- Respecter la charte, séparation stricte entre moteur (`src/`) et logique métier du jeu (`game/`), zéro dépendance externe.
- Vérifier systématiquement les tests et le packaging avant tout commit :
  ```
  build.bat tests
  build.bat
  build.bat kpack
  build\kpack.exe assets build\game.pak
  ```

## À faire

- port Linux
