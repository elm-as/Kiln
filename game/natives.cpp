// Lueur's own functions for KScript, on top of the engine's (src/script_api.h):
// the player, embers, arrows, checkpoints, the story, the bosses.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include "game.h"
#include "thermal.h"
#include "relics.h"

static World& W()
{
    return G.scene.world;
}

static Entity arg_entity(const Value* a, int n, int i)
{
    if (i >= n)
        return NO_ENTITY;
    Entity e = value_entity(a[i]);
    return W().alive(e) ? e : NO_ENTITY;
}

// getters on an entity that's gone give 0 instead of blowing up the script,
// a bat chasing a player who just died shouldn't have to care
template <class T>
static T* comp(const Value* a, int n, int i = 0)
{
    Entity e = arg_entity(a, n, i);
    return e ? W().get<T>(e) : 0;
}

static float argf(VM& vm, const Value* a, int n, int i, float def)
{
    if (i >= n || a[i].is_nil())
        return def;
    return (float)vm.num(a, n, i);
}

static bool argb(const Value* a, int n, int i, bool def)
{
    return i < n ? a[i].truthy() : def;
}

static bool tracing()
{
    static bool on = getenv("LUEUR_TRACE") != 0;
    return on;
}

static Value num(double d) { return Value::number(d); }
static Value nil() { return Value(); }

void natives_reset()
{
    script_api_reset();
}

static Value n_wind(VM& vm, const Value* a, int n)
{
    if (player_alive()) {
        Player* p = W().get<Player>(G.player);
        p->wind.x += argf(vm, a, n, 0, 0);
        p->wind.y += argf(vm, a, n, 1, 0);
    }
    return nil();
}

static Value n_steam_vent(VM& vm, const Value* a, int n)
{
    float x = argf(vm, a, n, 0, 0);
    float y = argf(vm, a, n, 1, 0);
    float w = argf(vm, a, n, 2, 32);
    float h = argf(vm, a, n, 3, 96);
    float power = argf(vm, a, n, 4, 360);
    int id = thermal_add_vent({ x, y }, w, h, power);
    return num(id);
}

static Value n_vent_set_active(VM& vm, const Value* a, int n)
{
    int id = (int)argf(vm, a, n, 0, -1);
    bool on = argb(a, n, 1, true);
    thermal_set_vent_active(id, on);
    return nil();
}

// boss_bar("name", 0..1) shows it, boss_bar() takes it away
static Value n_boss_bar(VM& vm, const Value* a, int n)
{
    if (n == 0) {
        G.boss_name.clear();
        return nil();
    }
    G.boss_name = vm.string_arg(a, n, 0);
    G.boss_hp = clampf(argf(vm, a, n, 1, 1), 0, 1);
    return nil();
}

// call every frame to keep the camera leaning this way
static Value n_cam_look(VM& vm, const Value* a, int n)
{
    camera_look(G.camera, { argf(vm, a, n, 0, 0), argf(vm, a, n, 1, 0) });
    return nil();
}

static Value n_lit_at(VM& vm, const Value* a, int n)
{
    return Value::boolean(lit_at({ argf(vm, a, n, 0, 0), argf(vm, a, n, 1, 0) }));
}

static Value n_has_power(VM& vm, const Value* a, int n)
{
    return Value::boolean(has_power(vm.string_arg(a, n, 0).c_str()));
}

static Value n_player(VM&, const Value*, int)
{
    return player_alive() ? entity_value(G.player) : nil();
}

static Value n_is_player(VM&, const Value* a, int n)
{
    Entity e = arg_entity(a, n, 0);
    return Value::boolean(e && e == G.player);
}

// kill_player() or kill_player("water") to be put out instead of blown up
static Value n_kill_player(VM&, const Value* a, int n)
{
    kill_player(n > 0 ? a[0].str().c_str() : "");
    return nil();
}

static Value n_bounce_player(VM& vm, const Value* a, int n)
{
    if (tracing())
        printf("bounce %.0f\n", argf(vm, a, n, 0, -250));
    bounce_player(argf(vm, a, n, 0, -250), argb(a, n, 1, true));
    return nil();
}

static Value n_shake(VM& vm, const Value* a, int n)
{
    add_shake(argf(vm, a, n, 0, 2));
    return nil();
}

static Value n_destroy(VM&, const Value* a, int n)
{
    Entity e = arg_entity(a, n, 0);
    if (e && e != G.player)
        W().destroy(e);
    return nil();
}

static Value n_collect_ember(VM&, const Value*, int)
{
    G.embers++;
    if (tracing())
        printf("ember %d/%d\n", G.embers, G.embers_total);
    return num(G.embers);
}

static Value n_add_arrows(VM& vm, const Value* a, int n)
{
    int before = G.quiver;
    int count = (int)argf(vm, a, n, 0, 3);
    // the forge's bonus: fires give a little more
    if (has_bonus("forge"))
        count += 2;
    add_arrows(count);
    return num(G.quiver - before);
}

static Value n_checkpoint(VM& vm, const Value* a, int n)
{
    if (tracing())
        printf("checkpoint\n");
    set_checkpoint({ argf(vm, a, n, 0, 0), argf(vm, a, n, 1, 0) });
    return nil();
}

static Value n_finish(VM&, const Value*, int)
{
    finish_level();
    return nil();
}

// same as finish, but it's the end of the world and the next level is in another
static Value n_finish_world(VM&, const Value*, int)
{
    G.world_done = true;
    finish_level();
    return nil();
}

static Value n_transition_to(VM& vm, const Value* a, int n)
{
    std::string level = vm.string_arg(a, n, 0);
    std::string tag = n > 1 ? vm.string_arg(a, n, 1) : "";
    return Value::boolean(transition_to(level, tag));
}

static Value n_relic_unlock(VM& vm, const Value* a, int n)
{
    std::string id = vm.string_arg(a, n, 0);
    return Value::boolean(relic_unlock(id.c_str()));
}

static Value n_relic_unlocked(VM& vm, const Value* a, int n)
{
    std::string id = vm.string_arg(a, n, 0);
    return Value::boolean(relic_is_unlocked(id.c_str()));
}

static Value n_relic_equipped(VM& vm, const Value* a, int n)
{
    std::string id = vm.string_arg(a, n, 0);
    return Value::boolean(relic_is_equipped(id.c_str()));
}

static Value n_relic_equip(VM& vm, const Value* a, int n)
{
    std::string id = vm.string_arg(a, n, 0);
    bool eq = argb(a, n, 1, true);
    return Value::boolean(relic_equip(id.c_str(), eq));
}

static Value n_save_game(VM&, const Value*, int)
{
    if (G.replay)
        return Value::boolean(false);
    if (tracing())
        printf("saved\n");
    return Value::boolean(save_game());
}

static Value n_say(VM& vm, const Value* a, int n)
{
    say(vm.string_arg(a, n, 0), argf(vm, a, n, 1, 0.15f));
    return nil();
}

static Value n_talk(VM& vm, const Value* a, int n)
{
    return Value::boolean(dialogue_start(vm.string_arg(a, n, 0)));
}

static Value n_time(VM&, const Value*, int)
{
    return num(G.level_time);
}

static Value n_cam_x(VM&, const Value*, int)
{
    return num(G.camera.pos.x);
}

static Value n_cam_y(VM&, const Value*, int)
{
    return num(G.camera.pos.y);
}

void register_natives()
{
    // the engine's own first (src/script_api.h), then Lueur's, which can
    // replace them: destroy here won't take the player away
    script_api_init(&G.scene, &G.tileset);
    VM& vm = script_vm();
    vm.def("player", n_player);
    vm.def("is_player", n_is_player);
    vm.def("wind", n_wind);
    vm.def("has_power", n_has_power);
    vm.def("boss_bar", n_boss_bar);
    vm.def("cam_look", n_cam_look);
    vm.def("lit_at", n_lit_at);
    vm.def("kill_player", n_kill_player);
    vm.def("bounce_player", n_bounce_player);
    vm.def("shake", n_shake);
    vm.def("destroy", n_destroy);
    vm.def("collect_ember", n_collect_ember);
    vm.def("checkpoint", n_checkpoint);
    vm.def("add_arrows", n_add_arrows);
    vm.def("finish", n_finish);
    vm.def("finish_world", n_finish_world);
    vm.def("save_game", n_save_game);
    vm.def("say", n_say);
    vm.def("time", n_time);
    vm.def("talk", n_talk);
    vm.def("cam_x", n_cam_x);
    vm.def("cam_y", n_cam_y);
    vm.def("steam_vent", n_steam_vent);
    vm.def("vent_set_active", n_vent_set_active);
    vm.def("transition_to", n_transition_to);
    vm.def("relic_unlock", n_relic_unlock);
    vm.def("relic_unlocked", n_relic_unlocked);
    vm.def("relic_equipped", n_relic_equipped);
    vm.def("relic_equip", n_relic_equip);
}
