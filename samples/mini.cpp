// A small platformer on kiln: as short as it gets while still using what a
// real game would. The level is a scene you can edit (F1), the hero is a png
// sheet exported from Aseprite, the rest is .pix, prefabs and three scripts.
// Seven coins to pick up.
//
//   build.bat mini, then build\mini.exe (from the project folder or build\)
//   build.bat mini release makes build\mini.exe + build\mini.pak to share
//
// Everything under samples/mini/assets reloads while it runs.

#include <cstdio>
#include "kiln.h"

const int W = 320, H = 180;
enum { LEFT, RIGHT, JUMP };

static Scene scene;
static Tileset tileset;
static PhysicsConfig physics;
static std::vector<Contact> contacts;
static Camera camera;
static Font font;
static Entity hero;
static vec2 start;
static int coins, coins_total;
static bool editing;
static std::string asset_dir = "samples/mini/assets";

static void setup_level()
{
    script_api_reset();
    hero = NO_ENTITY;
    coins = coins_total = 0;
    scene.world.each<Info, Transform>([&](Entity e, Info& info, Transform& t) {
        if (info.prefab == "hero") {
            hero = e;
            start = t.pos;
        } else if (info.prefab == "coin") {
            coins_total++;
        }
    });
    float ts = (float)tileset.size;
    camera = Camera();
    camera.size = { (float)W, (float)H };
    camera.bounds = { 0, 0, scene.map.w * ts, scene.map.h * ts };
    camera_snap(camera, start);
}

static void load_level()
{
    Json j;
    if (!asset_json("scenes/level.json", &j) || !scene_load(scene, j)) {
        fprintf(stderr, "no level\n");
        return;
    }
    scene.name = "level";
    setup_level();
}

static void load_data()
{
    Json j;
    if (asset_json("data/tileset.json", &j))
        tileset.load(j);
    if (asset_json("data/prefabs.json", &j))
        prefabs_load(j);
    if (asset_json("data/particles.json", &j))
        particles_load(j);
}

// back to the start, a bit shaken
static void respawn()
{
    Transform* t = scene.world.get<Transform>(hero);
    Body* b = scene.world.get<Body>(hero);
    if (!t || !b)
        return;
    t->pos = t->prev = start;
    b->vel = { 0, 0 };
    camera_shake(camera, 3);
    audio_play(sfx("hurt"), 0.6f);
}

// what the scripts get on top of the engine's own functions
static Value n_is_player(VM&, const Value* a, int n)
{
    return Value::boolean(hero && script_arg_entity(a, n, 0) == hero);
}

static Value n_collect(VM&, const Value*, int)
{
    coins++;
    return Value::number(coins);
}

static Value n_hurt(VM&, const Value*, int)
{
    respawn();
    return Value();
}

static void draw_world(Scene& s, Rect view)
{
    tilemap_draw(s.map, tileset, view, WHITE, false, engine_time());
    draw_sprites(s.world, view, 1);
    particles_draw(false);
    particles_draw(true);
}

static void init()
{
    scripting_init();
    register_engine_components();
    // from the project folder, from build\ (a double click), or next to mini.pak
    if (!dir_exists(asset_dir.c_str()))
        asset_dir = "../" + asset_dir;
    if (!assets_init(asset_dir.c_str(), "mini.pak")) {
        fprintf(stderr, "no assets (need samples/mini/assets or mini.pak)\n");
        engine_quit();
        return;
    }
    load_data();

    script_api_init(&scene, &tileset);
    VM& vm = script_vm();
    vm.def("is_player", n_is_player);
    vm.def("collect", n_collect);
    vm.def("hurt", n_hurt);

    action_define("gauche", "gauche", { KEY_LEFT, KEY_Q, KEY_A });
    action_define("droite", "droite", { KEY_RIGHT, KEY_D });
    action_define("saut", "saut", { KEY_SPACE, KEY_Z, KEY_W });

    font = font_create("Consolas", 12);
    EditorHooks hooks = {};
    hooks.draw_world = draw_world;
    hooks.tileset = &tileset;
    editor_init(hooks);
    load_level();
}

static void frame(const Input& in, float dt)
{
    if (assets_poll()) {
        load_data();
        scripting_reload();
    }
    // F1 edits the level as it is, F1 again plays what's there now
    if (in.pressed[KEY_F1] && assets_dev() && !ui_want_keyboard()) {
        editing = !editing;
        if (editing) {
            editor_open(&scene, asset_dir + "/scenes/level.json");
        } else {
            editor_close();
            Json j = scene_save(scene);
            scene_load(scene, j);
            setup_level();
        }
    }
    if (editing)
        editor_frame(in, dt);
}

static void update(const Input& in, float dt)
{
    if (editing || !hero)
        return;
    World& w = scene.world;
    Body& b = *w.get<Body>(hero);
    Transform& t = *w.get<Transform>(hero);

    int dir = (action_down(in, RIGHT) ? 1 : 0) - (action_down(in, LEFT) ? 1 : 0);
    b.vel.x = dir * 100.f;
    if (action_pressed(in, JUMP) && b.on_ground) {
        b.vel.y = -300;
        audio_play(sfx("jump"), 0.5f, randf(0.95f, 1.05f));
    }
    // let go early for a smaller hop
    if (!action_down(in, JUMP) && b.vel.y < -120)
        b.vel.y = -120;
    if (dir)
        w.get<Sprite>(hero)->flip_x = dir < 0;
    anim_play(w, hero, dir ? "run" : "idle");
    if (t.pos.y > scene.map.h * tileset.size + 32)
        respawn();

    scripts_update(w, dt);
    physics_step(w, scene.map, tileset, physics, dt, contacts);
    for (const Contact& c : contacts) {
        if (w.alive(c.a) && w.has<Script>(c.a))
            script_send(w, c.a, "touch", { entity_value(c.b) });
        if (w.alive(c.b) && w.has<Script>(c.b))
            script_send(w, c.b, "touch", { entity_value(c.a) });
    }
    anim_update(w, dt);
    particles_update(w, dt);
    w.flush();
    camera_update(camera, t.pos - vec2{ 0, 16 }, dt);
}

static void draw(float alpha)
{
    if (editing) {
        render_screen_begin();
        editor_draw();
        return;
    }
    vec2 cam = camera_view(camera, alpha);
    render_set_view(cam);
    Rect view = { cam.x, cam.y, (float)W, (float)H };
    tilemap_draw(scene.map, tileset, view, WHITE, false, engine_time());
    draw_sprites(scene.world, view, alpha);
    particles_draw(false);
    particles_draw(true);

    render_set_view({ 0, 0 });
    char buf[64];
    snprintf(buf, sizeof(buf), "pièces %d / %d", coins, coins_total);
    draw_text(font, buf, 6, 4, rgb(0xffd84a));
    std::string hint = coins == coins_total ? "toutes ! bravo" : action_text(assets_dev() ? "{gauche} {droite} {saut}   F1 éditeur" : "{gauche} {droite} {saut}");
    draw_text(font, hint.c_str(), 6, H - 16.f, rgb(0xd8e0f0));
}

int main()
{
    GameDesc g = {};
    g.title = "kiln mini";
    g.width = W;
    g.height = H;
    g.scale = 4;
    g.clear = rgb(0x6fa8dc);
    g.init = init;
    g.frame = frame;
    g.update = update;
    g.draw = draw;
    return engine_run(g);
}
