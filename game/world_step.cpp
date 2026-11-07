// Per-tick world simulation: player spawning, cold/heat survival, death/respawn,
// level completion, and frame update orchestration.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game.h"
#include "relics.h"
#include "thermal.h"

vec2 cam_focus()
{
    if (!player_alive())
        return G.spawn;
    World& w = G.scene.world;
    Transform* t = w.get<Transform>(G.player);
    Player* p = w.get<Player>(G.player);
    return t->pos + vec2{ p->facing * 24.f, -24 };
}

void spawn_player()
{
    World& w = G.scene.world;
    G.player = prefab_spawn(w, "player", G.spawn);
    w.get<Info>(G.player)->runtime = true;

    const WorldInfo& world = current_world();
    if (Light* l = w.get<Light>(G.player)) {
        l->radius *= world.light * (has_bonus("ciel") ? 1.2f : 1);
        if (world.aura.a)
            l->color = world.aura;
        if (world.light < 1)
            l->flicker *= 2.5f;
        G.light_base = l->radius;
    }
    G.heat = 1;
    if (has_bonus("ombres"))
        if (Sprite* s = w.get<Sprite>(G.player))
            s->tint = rgb(0xffe070);
}

static bool near_fire(vec2 p)
{
    World& w = G.scene.world;
    bool warm = false;
    w.each<Info, Light>([&](Entity e, Info& info, Light& l) {
        if (warm || l.radius <= 0)
            return;
        const std::string& k = info.prefab;
        if (k != "brazier" && k != "torche" && k != "torch" && k != "hearth" && k != "feu")
            return;
        vec2 d = w.get<Transform>(e)->pos - p;
        if (d.x * d.x + d.y * d.y < 46 * 46)
            warm = true;
    });
    return warm;
}

static void update_heat(float dt)
{
    const WorldInfo& world = current_world();
    if (world.cold <= 0 || !player_alive() || G.finish_timer > 0)
        return;
    World& w = G.scene.world;
    vec2 p = w.get<Transform>(G.player)->pos;
    float before = G.heat;
    if (near_fire(p)) {
        G.heat = fminf(1, G.heat + 0.9f * dt);
        if (G.has[WEAPON_BOW] && G.quiver < 8) {
            G.arrow_regen += dt;
            if (G.arrow_regen > 1.2f) {
                G.arrow_regen = 0;
                add_arrows(1);
                audio_play(sfx("quiver"), 0.25f, 1.4f);
            }
        }
    } else {
        float cold_rate = (has_bonus("volcan") ? 0.7f : 1.0f);
        if (relic_is_equipped("coeur_cendre"))
            cold_rate *= 0.4f;
        G.heat = fmaxf(0, G.heat - world.cold * cold_rate * dt);
    }
    if (G.heat >= 1 && before < 1)
        audio_play(sfx("checkpoint"), 0.3f, 1.4f);

    if (Light* l = w.get<Light>(G.player)) {
        l->radius = G.light_base * (0.3f + 0.7f * G.heat);
        l->flicker = G.heat < 0.3f ? 0.5f : 0.15f;
    }
    if (G.heat < 0.3f) {
        G.shiver -= dt;
        if (G.shiver <= 0) {
            G.shiver = 0.25f + G.heat * 2;
            audio_play(sfx("shiver"), 0.35f, randf(0.9f, 1.1f));
            particles_burst(particle_fx("frost"), p - vec2{ 0, 6 }, 2);
        }
    }
    if (G.heat <= 0)
        kill_player("cold");
}

void kill_player(const char* how)
{
    if (!player_alive() || G.finish_timer > 0)
        return;
    World& w = G.scene.world;
    vec2 p = w.get<Transform>(G.player)->pos;
    printf("died at %.0f,%.0f after %.1fs%s%s\n", p.x, p.y, G.level_time, *how ? ", " : "", how);
    if (!strcmp(how, "shadow")) {
        particles_burst(particle_fx("shadow_mist"), p - vec2{ 0, 6 }, 20);
        audio_play(sfx("shade_die"), 0.8f, 0.6f);
    } else if (!strcmp(how, "cold")) {
        particles_burst(particle_fx("frost"), p - vec2{ 0, 6 }, 16);
        audio_play(sfx("freeze"), 0.8f);
    } else if (!strcmp(how, "water")) {
        particles_burst(particle_fx("steam"), p - vec2{ 0, 4 });
        audio_play(sfx("sizzle"), 0.8f);
    } else {
        particles_burst(particle_fx("death"), p - vec2{ 0, 6 });
        particles_burst(particle_fx("embers_die"), p - vec2{ 0, 6 });
        audio_play(sfx("die"), 0.8f);
    }
    add_shake(4);
    engine_hitstop(0.09f);
    w.destroy(G.player);
    G.player = NO_ENTITY;
    G.deaths++;
    G.respawn_timer = 1.1f;
    tween(&G.fade, 1, 0.35f, EASE_IN_QUAD, 0.55f);
}

static void respawn()
{
    if (G.has[WEAPON_BOW])
        G.quiver = std::max(G.quiver, 3);
    spawn_player();
    camera_snap(G.camera, cam_focus());
    tween(&G.fade, 0, 0.35f);
    particles_burst(particle_fx("respawn"), G.spawn - vec2{ 0, 6 });
    audio_play(sfx("respawn"), 0.5f);
}

void finish_level()
{
    if (G.finish_timer > 0)
        return;
    G.finish_timer = 1.8f;
    music_stop(1.5f);
    printf("%s done in %.1fs, %d/%d embers, %d deaths\n", G.scene.name.c_str(), G.level_time,
           G.embers, G.embers_total, G.deaths);
}

void enter_clear()
{
    G.bonus_msg.clear();
    const std::string& name = G.levels[G.level];
    const WorldInfo& world = current_world();
    bool had = has_bonus(world.id.c_str());
    if (G.embers > G.best[name])
        G.best[name] = G.embers;
    if (!had && has_bonus(world.id.c_str()))
        G.bonus_msg = std::string("bonus : ") + bonus_text(world.id.c_str());
    save_progress();

    G.run_time += G.level_time;
    G.run_deaths += G.deaths;
    G.run_embers += G.embers;
    G.run_embers_total += G.embers_total;
    G.mode = MODE_CLEAR;
    G.menu = 0;
    audio_play(sfx("clear"), 0.7f);
}

void bounce_player(float vy, bool can_cut)
{
    if (!player_alive())
        return;
    World& w = G.scene.world;
    w.get<Body>(G.player)->vel.y = vy;
    Player* p = w.get<Player>(G.player);
    p->jumping = can_cut;
    p->coyote = 0;
    p->squash = -0.3f;
}

void set_checkpoint(vec2 p)
{
    G.spawn = p;
}

void say(const std::string& text, float seconds)
{
    G.message = action_text(text);
    G.message_time = seconds;
}

void add_shake(float amount)
{
    camera_shake(G.camera, amount);
}

void world_update(const Input& in, float dt)
{
    World& w = G.scene.world;

    if (player_alive() && G.finish_timer <= 0)
        G.level_time += dt;

    static const Input nothing = {};
    player_update(G.finish_timer > 0 ? nothing : in, dt);

    scripts_update(w, dt);
    physics_step(w, G.scene.map, G.tileset, G.physics, dt, G.contacts);

    for (const Contact& c : G.contacts) {
        if (w.alive(c.a) && w.has<Script>(c.a))
            script_send(w, c.a, "touch", { entity_value(c.b) });
        if (w.alive(c.b) && w.has<Script>(c.b))
            script_send(w, c.b, "touch", { entity_value(c.a) });
    }

    arrows_update(dt);
    update_heat(dt);
    G.eclat_glow = fmaxf(0, G.eclat_glow - dt / 3);
    anim_update(w, dt);
    particles_update(w, dt);
    thermal_update(dt);

    static int trace = getenv("LUEUR_TRACE") ? atoi(getenv("LUEUR_TRACE")) : 0;
    if (trace && player_alive() && (trace > 1 || engine_ticks() % 15 == 0)) {
        Transform* t = w.get<Transform>(G.player);
        Body* b = w.get<Body>(G.player);
        printf("tick %llu  x %.0f y %.0f  tile %d,%d  vel %.0f,%.0f%s", (unsigned long long)engine_ticks(),
               t->pos.x, t->pos.y, (int)(t->pos.x / 16), (int)((t->pos.y - 1) / 16), b->vel.x, b->vel.y,
               b->on_ground ? "  ground" : "");
        if (current_world().cold > 0)
            printf("  heat %.2f", G.heat);
        printf("\n");
    }

    camera_update(G.camera, cam_focus(), dt);

    if (G.message_time > 0)
        G.message_time -= dt;

    if (!player_alive() && G.respawn_timer > 0) {
        G.respawn_timer -= dt;
        if (G.respawn_timer <= 0)
            respawn();
    }

    if (G.finish_timer > 0) {
        G.finish_timer -= dt;
        if (G.finish_timer <= 0) {
            const char* outro = G.scene.props["outro"].str("");
            if (*outro && !G.playtest && dialogue_start(outro))
                G.clear_after_talk = true;
            else
                enter_clear();
        }
    }

    w.flush();
}

