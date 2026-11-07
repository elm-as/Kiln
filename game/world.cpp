#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game.h"
#include "thermal.h"
#include "relics.h"

Game G;

void game_load_data()
{
    Json cfg;
    if (asset_json("data/game.json", &cfg))
        G.config = cfg;

    G.worlds.clear();
    G.levels.clear();
    G.level_world.clear();
    for (const Json& wj : G.config["worlds"].arr) {
        WorldInfo w;
        w.id = wj["id"].str();
        w.name = wj["name"].str();
        w.intro = wj["intro"].str();
        w.run = wj["run"].f(1);
        w.jump = wj["jump"].f(1);
        w.light = wj["light"].f(1);
        w.aura = json_color(wj["aura"], CLEAR);
        w.cold = wj["cold"].f(0);
        for (const Json& l : wj["levels"].arr) {
            w.levels.push_back(l.s);
            G.levels.push_back(l.s);
            G.level_world.push_back((int)G.worlds.size());
        }
        G.worlds.push_back(w);
    }

    const Json& p = G.config["player"];
    Tuning& t = G.tune;
#define TUNE(name) t.name = p[#name].f(t.name)
    TUNE(run); TUNE(accel); TUNE(air_accel); TUNE(friction); TUNE(air_friction);
    TUNE(jump); TUNE(jump_cut); TUNE(fall_gravity); TUNE(apex_gravity);
    TUNE(wall_slide); TUNE(wall_jump_x); TUNE(wall_jump_y);
    TUNE(coyote); TUNE(buffer);
#undef TUNE
    G.physics.gravity = G.config["gravity"].f(900);

    Json j;
    if (asset_json("data/tileset.json", &j))
        G.tileset.load(j);
    if (asset_json("data/prefabs.json", &j))
        prefabs_load(j);
    if (asset_json("data/particles.json", &j))
        particles_load(j);
    story_load();

    // what the world list needs to know about levels without loading them
    G.level_embers.assign(G.levels.size(), 0);
    G.level_titles.assign(G.levels.size(), "");
    for (size_t i = 0; i < G.levels.size(); i++) {
        Json s;
        if (!asset_json("scenes/" + G.levels[i] + ".json", &s))
            continue;
        for (const Json& e : s["entities"].arr)
            if (!strcmp(e["prefab"].str(""), "ember"))
                G.level_embers[i]++;
        std::string title = s["props"]["title"].str(G.levels[i].c_str());
        size_t dot = title.find("· ");
        G.level_titles[i] = dot == std::string::npos ? title : title.substr(dot + strlen("· "));
    }
}

void world_embers(int world, int* got, int* total)
{
    *got = *total = 0;
    for (size_t i = 0; i < G.levels.size(); i++) {
        if (G.level_world[i] != world)
            continue;
        *total += G.level_embers[i];
        *got += best_embers((int)i);
    }
}

int best_embers(int level)
{
    auto it = G.best.find(G.levels[level]);
    return it == G.best.end() ? 0 : std::min(it->second, G.level_embers[level]);
}

static const struct { const char* world; const char* text; } bonuses[] = {
    { "grotte", "carquois agrandi, 15 flèches" },
    { "foret", "plume : on plane plus lentement" },
    { "volcan", "cœur de lave : le froid mord moins" },
    { "mer", "éclat qui revient plus vite" },
    { "glace", "élan plus long" },
    { "ciel", "une lumière plus grande" },
    { "forge", "deux flèches de plus à chaque feu" },
    { "ombres", "Lueur toute dorée" },
};

const char* bonus_text(const char* world)
{
    for (const auto& b : bonuses)
        if (!strcmp(b.world, world))
            return b.text;
    return "";
}

bool has_bonus(const char* world)
{
    for (size_t w = 0; w < G.worlds.size(); w++) {
        if (G.worlds[w].id != world)
            continue;
        int got, total;
        world_embers((int)w, &got, &total);
        return total > 0 && got >= total;
    }
    return false;
}

int quiver_max()
{
    return QUIVER_MAX + (has_bonus("grotte") ? 3 : 0);
}

void load_parallax(const Json& props)
{
    const Json& layers = props["parallax"].is_arr() ? props["parallax"] : G.config["parallax"];
    G.parallax = parallax_load(layers);
}

const WorldInfo& current_world()
{
    static const WorldInfo none;
    if (G.level < 0 || G.level >= (int)G.level_world.size())
        return none;
    return G.worlds[G.level_world[G.level]];
}

Tuning world_tuning()
{
    const WorldInfo& w = current_world();
    Tuning t = G.tune;
    t.run *= w.run;
    t.accel *= w.run;
    t.air_accel *= w.run;
    t.wall_jump_x *= w.run;
    t.jump *= w.jump;
    t.wall_jump_y *= w.jump;
    return t;
}

bool level_load(int index)
{
    if (index < 0 || index >= (int)G.levels.size())
        return false;
    Json j;
    if (!asset_json("scenes/" + G.levels[index] + ".json", &j))
        return false;
    G.level = index;
    G.scene.name = G.levels[index];
    if (!G.replay && index > G.reached) {
        G.reached = index;
        save_progress();
    }
    scene_load(G.scene, j);
    level_begin();
    return true;
}

void level_begin()
{
    World& w = G.scene.world;

    vec2 start = { 32, 32 };
    int embers = 0;
    w.each<Info>([&](Entity e, Info& info) {
        if (info.prefab == "start") {
            bool use = true;
            if (!G.next_spawn_tag.empty()) {
                if (!info.name.empty() && info.name != G.next_spawn_tag) {
                    use = false;
                } else if (w.has<Script>(e)) {
                    Script* sc = w.get<Script>(e);
                    std::string t = sc->props["tag"].str("");
                    if (!t.empty() && t != G.next_spawn_tag)
                        use = false;
                }
            }
            if (use) {
                start = w.get<Transform>(e)->pos;
            }
            w.destroy(e);
        } else if (info.prefab == "ember") {
            embers++;
        }
    });
    G.next_spawn_tag.clear();
    w.flush();

    G.spawn = start;
    // LUEUR_SPAWN=x,y starts somewhere else, same idea as shift+F5 in the editor
    if (const char* sp = getenv("LUEUR_SPAWN"))
        sscanf(sp, "%f,%f", &G.spawn.x, &G.spawn.y);
    G.embers = 0;
    G.embers_total = embers;
    G.arrows.clear();
    G.boss_name.clear();
    G.eclat_glow = 0;
    G.eclat_cool = 0;
    G.deaths = 0;
    G.level_time = 0;
    G.respawn_timer = 0;
    G.finish_timer = 0;
    G.message.clear();
    G.clear_after_talk = false;
    G.world_done = false;
    particles_clear();
    tween_clear();
    script_clear_error();
    natives_reset();
    thermal_reset();

    spawn_player();
    float ts = (float)G.tileset.size;
    G.camera = Camera();
    G.camera.size = { (float)VIEW_W, (float)VIEW_H };
    G.camera.bounds = { 0, 0, G.scene.map.w * ts, G.scene.map.h * ts };
    camera_snap(G.camera, cam_focus());
    G.fade = 1;
    tween(&G.fade, 0, 0.6f);
    G.banner = G.scene.props["title"].str(G.scene.name.c_str());
    load_parallax(G.scene.props);

    const char* song = G.scene.props["music"].str("");
    if (*song)
        music_play(music(song), 0.5f, 1);
    G.mode = MODE_PLAY;
}

bool transition_to(const std::string& level_name, const std::string& spawn_tag)
{
    for (size_t i = 0; i < G.levels.size(); i++) {
        if (G.levels[i] == level_name) {
            if (G.level >= 0 && G.level < (int)G.levels.size()) {
                const std::string& cur = G.levels[G.level];
                if (G.embers > G.best[cur])
                    G.best[cur] = G.embers;
            }
            if (!G.replay && (int)i > G.reached)
                G.reached = (int)i;
            save_progress();
            G.next_spawn_tag = spawn_tag;
            G.level = (int)i;
            level_load((int)i);
            return true;
        }
    }
    return false;
}
