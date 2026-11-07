# Faire un jeu avec kiln

Ce qu'il faut savoir pour partir d'une page blanche. L'exemple complet le plus
court est `samples/mini.cpp` (un petit plateformer, ~250 lignes) avec ses
assets dans `samples/mini/assets`. Lueur (`game/`) montre tout le reste.

## Le squelette

Un jeu, c'est une `GameDesc` et cinq fonctions :

```cpp
#include "kiln.h"

static void init() { ... }                          // une fois
static void frame(const Input& in, float dt) { }    // chaque image : menus, éditeur
static void update(const Input& in, float dt) { }   // à pas fixe, la simulation
static void draw(float alpha) { }                   // alpha : entre deux ticks

int main()
{
    GameDesc g = {};
    g.title = "mon jeu";
    g.width = 320;          // la toile, agrandie en entier dans la fenêtre
    g.height = 180;
    g.scale = 4;
    g.tick_rate = 60;       // 60 par défaut
    g.clear = rgb(0x6fa8dc);
    g.init = init;
    g.frame = frame;
    g.update = update;
    g.draw = draw;
    return engine_run(g);
}
```

Dans `init`, dans cet ordre :

```cpp
scripting_init();
register_engine_components();
assets_init("mon/dossier/assets", "jeu.pak");
// tileset, prefabs, particules depuis data/
script_api_init(&scene, &tileset);      // les fonctions de base des scripts
// puis tes propres fonctions : script_vm().def("nom", fonction)
```

La boucle d'`update` d'un jeu à entités ressemble toujours à ça :

```cpp
scripts_update(w, dt);
physics_step(w, scene.map, tileset, physics, dt, contacts);
for (const Contact& c : contacts) {     // touch(self, other) des deux côtés
    if (w.alive(c.a) && w.has<Script>(c.a)) script_send(w, c.a, "touch", { entity_value(c.b) });
    if (w.alive(c.b) && w.has<Script>(c.b)) script_send(w, c.b, "touch", { entity_value(c.a) });
}
anim_update(w, dt);
particles_update(w, dt);
w.flush();                              // les destroy() prennent effet ici
camera_update(camera, focus, dt);
```

Et `draw` :

```cpp
vec2 cam = camera_view(camera, alpha);
render_set_view(cam);
tilemap_draw(scene.map, tileset, view);
draw_sprites(scene.world, view, alpha);
particles_draw(false);
particles_draw(true);                   // les additives par-dessus
render_set_view({ 0, 0 });              // l'interface, en coordonnées écran
```

## Les assets

```
assets/
  sprites/    .pix, .png ou .tga (+ .json à côté)
  sounds/     .sfx (synthé) ou .wav
  music/      .song (synthé)
  scripts/    .ks
  scenes/     .json, un niveau par fichier
  data/       tileset.json, prefabs.json, particles.json, et ce que tu veux
```

Tant que le dossier est là, le jeu prépare tout au démarrage et **recharge
dès qu'un fichier change** : sprites, sons, scripts, niveaux. Pour livrer,
`kpack assets jeu.pak` fait un seul fichier, que `assets_init` prend quand le
dossier n'est pas là. La préparation vérifie aussi que les noms utilisés dans
les scripts et les prefabs existent (sprites, sons, prefabs, effets).

## Les sprites

### .pix : du pixel art en texte

```
size 8 8
origin 4 8            le point qui suit la position de l'entité
palette               optionnel, s'ajoute à sprites/palette.txt
x #ff0044
frame idle
..xxxx..
(8 lignes)
frame blink
...
anim idle 4 loop idle blink      nom, images/s, loop ou once, images
```

### .png avec une grille

Une planche d'images de même taille, et un `.json` du même nom à côté :

```json
{ "size": [16, 16], "origin": [8, 16],
  "names": ["idle0", "idle1", "run0"],
  "anims": { "run": { "fps": 10, "loop": true, "frames": [2, "idle0"] } } }
```

C'est ce que donne l'export « spritesheet » de Pixelorama, ou une planche
faite à la main. Sans `.json`, l'image entière est une seule image.

### .png exporté d'Aseprite

*Fichier > Export Sprite Sheet*, cocher *JSON Data* (hash ou array, les deux
marchent), et poser le `.png` et le `.json` dans `sprites/` avec le même nom.
Les tags deviennent des animations (sens avant, arrière, aller-retour), la
durée des images donne la vitesse, les images rognées sont remises à leur
place. L'origine est le pivot de la première slice s'il y en a une, sinon un
`"origin": [x, y]` ajouté au fichier, sinon le milieu du bas de l'image.
`samples/mini/assets/sprites/hero.json` en est un.

## Les tuiles

`data/tileset.json` : un caractère par tuile.

```json
{ "size": 16, "tiles": {
    "#": { "sprite": "tiles", "frames": ["dirt"], "top": "grass", "solid": true, "edges": true },
    "=": { "sprite": "tiles", "frames": ["ledge"], "oneway": true },
    "~": { "sprite": "tiles", "frames": ["water"], "top": ["w0", "w1"], "fps": 3, "water": true, "box": [0, 7, 16, 9] }
} }
```

`top` : l'image quand rien n'est au-dessus. `edges` : un bord sombre là où la
tuile touche le vide. Drapeaux : `solid`, `oneway` (solide seulement par le
dessus), `hazard`, `lava`, `water`, `ice`. `box` restreint la zone qui compte
pour ces drapeaux. `light` et `light_color` font briller la tuile.

## Prefabs et composants

`data/prefabs.json` décrit chaque sorte d'entité par ses composants :

```json
"coin": {
  "Sprite": { "sprite": "coin", "layer": 5 },
  "Animator": { "play": "spin" },
  "Collider": { "size": [8, 8], "layer": 4, "mask": 0, "trigger": true },
  "Script": { "module": "coin", "props": { "value": 1 } }
}
```

- **Sprite** : `sprite`, `frame`, `layer` (plus haut = devant), `offset`, `tint`
- **Animator** : `play`, l'animation de départ
- **Body** : `type` (`dynamic`, `kinematic`, `static`), `gravity`, `friction`,
  `max_fall`, `mass`
- **Collider** : `size` ou `radius`, `offset`, `layer`, `mask`, `trigger`, `oneway`
- **Light** : `radius`, `color`, `flicker`, `offset`
- **Emitter** : `fx` (un effet de `particles.json`), `on`, `offset`
- **Script** : `module` (`scripts/<nom>.ks`), `props`

La physique : deux colliders se touchent si le `layer` de l'un est dans le
`mask` de l'autre. Dans les deux sens, ils se poussent. Dans un seul sens, le
contact est juste signalé (c'est comme ça qu'un joueur sent un ennemi sans le
pousser). Un corps `kinematic` bougé par sa vitesse emporte ce qui est posé
dessus : c'est ce qu'il faut pour les plateformes mobiles.

## Scènes et éditeur

Une scène : `props` (ce que tu veux : musique, titre, lumière…), `tiles` et
`back` (lignes de caractères), `entities` (`prefab`, `pos`, et `set` pour
changer des valeurs du prefab).

L'éditeur s'ouvre sur une scène avec `editor_open(&scene, chemin)` et se
dessine avec `editor_draw()` (voir `samples/mini.cpp`, touche F1). Dedans :
1/2/3 pour tuiles, fond et entités, clic gauche pour peindre ou poser, clic
droit pour effacer ou choisir, ctrl+z/y, ctrl+s, ctrl+d duplique, suppr, f
cadre tout, molette pour zoomer, milieu ou espace + glisser pour se déplacer.
La liste des prefabs se filtre, et un fantôme montre ce qu'un clic posera.

Quelques propriétés de script veulent dire quelque chose pour l'éditeur :

- `link` : un nom d'interrupteur partagé. Les entités qui ont le même sont
  reliées par un trait de la même couleur (le levier et sa porte).
- `dx`, `dy` : un trajet depuis la position. Il est dessiné en orange.
- `w`, `h` : une zone au-dessus des pieds de l'entité (vent, déclencheurs).

Les entités sans sprite ont un cadre bleu et leur nom, pour qu'on les voie.

## Les scripts

Un module `scripts/coin.ks` peut définir :

```
fn init(self)               une fois, au premier tick
fn update(self, dt)         chaque tick
fn touch(self, other)       quand son collider en touche un autre
```

Et tout ce que le jeu envoie avec `script_send` (Lueur envoie par exemple
`hit(self, dx, dy)` quand la lance ou une flèche touche). `self` contient les
`props` du prefab et `self.id`, l'entité.

Le langage :

```
let a = 1
fn f(x) { return x * 2 }
if a == 1 { } elif a > 2 { } else { }
while a < 10 { a += 1 }
for i in 0..5 { }              0 à 4
for k, v in table { }
let l = [1, 2]    push(l, 3)    len(l)
let t = { x: 1 }  t.y = 2
nil  true  false  and  or  not      "a" + 1 donne "a1"
```

Fonctions de base : `print len push pop remove keys has str num type floor
ceil round abs sqrt sin cos atan2 sign min max clamp lerp rand randi`, et `pi`.

Celles du moteur, avec `script_api_init` (détails dans `src/script_api.h`) :
`x y set_pos vel_x vel_y set_vel set_gravity on_ground on_wall ground_of anim
anim_done flip flash visible light emit collide alive spawn destroy find dist
solid floor_at sfx burst hitstop flag set_flag`. Une entité disparue se lit
comme 0 au lieu de planter le script.

`flag(nom)` et `set_flag(nom, oui)` sont des interrupteurs communs à tous les
scripts d'un niveau : un levier en allume un, une porte le lit. Ils se vident
avec `script_api_reset()`, à appeler à chaque début de niveau.

Pour ajouter les tiennes :

```cpp
static Value n_collect(VM& vm, const Value* a, int n)
{
    coins += (int)script_argf(vm, a, n, 0, 1);
    return Value::number(coins);
}
script_vm().def("collect", n_collect);
```

Redéfinir un nom du moteur le remplace (Lueur a son propre `destroy`, qui
refuse d'effacer le joueur).

## Touches

```cpp
enum { LEFT, RIGHT, JUMP };     // dans l'ordre des action_define
action_define("gauche", "gauche", { KEY_LEFT, KEY_Q, KEY_A });
action_define("droite", "droite", { KEY_RIGHT, KEY_D });
action_define("saut", "saut", { KEY_SPACE });

if (action_pressed(in, JUMP)) ...
action_bind(JUMP, KEY_K);                   // K en premier, retiré des autres actions
Json j = actions_save(); actions_load(j);   // pour un fichier d'options
action_text("{saut} pour sauter")           // "k pour sauter"
```

## Caméra

```cpp
Camera cam;
cam.size = { 320, 180 };
cam.bounds = { 0, 0, largeur_niveau, hauteur_niveau };
camera_snap(cam, joueur);            // au début
camera_update(cam, joueur, dt);      // chaque tick
camera_shake(cam, 3);
camera_look(cam, boss);              // tire vers le boss, pour ce tick
vec2 vue = camera_view(cam, alpha);  // à passer à render_set_view
```

## Sons et musique

Un `.sfx`, une clé par ligne :

```
wave square          square saw triangle sine noise
freq 440 -> 880      glisse du premier au second
duty 0.25
attack 0.01          en secondes
sustain 0.05
decay 0.2
volume 0.5
vibrato 8 0.03       vitesse, profondeur
arp 1.5 0.06         multiplie la hauteur par 1.5 après 0.06 s
```

`audio_play(sfx("coin"), volume, hauteur)` le joue. `music_play(music("nom"))`
joue un `.song`, avec fondu. `audio_groups(musique, effets)` règle les deux
volumes à part.

Un `.song` : `tempo`, `rows` (cases par temps), des `inst nom onde vol …
attack … decay … sustain … release … vibrato …`, des `pattern` avec une ligne
par instrument (`C4`, `F#5`… ; `.` rien ; `-` coupe la note ; `|` pour
aérer), et un `order` qui enchaîne les patterns. Ceux de `assets/music` sont
de bons modèles.

## Tester sans jouer

- `KILN_INPUT=fichier` rejoue des touches : `120 right down`, `600 shot
  capture.png`, `900 quit`. Les ticks sont fixes, donc une partie se rejoue
  toujours à l'identique.
- `KILN_SHOT=capture.png` prend une image (la 60e, ou `KILN_SHOT_FRAME`) et quitte.
- `build.bat tests` lance les tests du moteur (langage de script, JSON, ECS,
  PNG, touches, caméra, import des sprites).
