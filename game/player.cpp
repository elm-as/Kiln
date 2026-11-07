#include <algorithm>
#include <cmath>
#include <cstdio>
#include "game.h"
#include "relics.h"

// keyboard through the bindings in options.cpp, the pad as it comes
bool input_left(const Input& in)
{
    return action_down(in, ACT_LEFT) || in.pad_down[PAD_LEFT] || in.pad_lx < -0.4f;
}

bool input_right(const Input& in)
{
    return action_down(in, ACT_RIGHT) || in.pad_down[PAD_RIGHT] || in.pad_lx > 0.4f;
}

bool input_down(const Input& in)
{
    return action_down(in, ACT_DOWN) || in.pad_down[PAD_DOWN] || in.pad_ly > 0.6f;
}

bool input_up(const Input& in)
{
    return action_down(in, ACT_UP) || in.pad_down[PAD_UP] || in.pad_ly < -0.6f;
}

bool input_jump_pressed(const Input& in)
{
    return action_pressed(in, ACT_JUMP) || in.pad_pressed[PAD_A];
}

bool input_jump_held(const Input& in)
{
    return action_down(in, ACT_JUMP) || in.pad_down[PAD_A];
}

bool input_attack(const Input& in)
{
    return action_pressed(in, ACT_ATTACK) || in.pad_pressed[PAD_X];
}

bool input_attack_held(const Input& in)
{
    return action_down(in, ACT_ATTACK) || in.pad_down[PAD_X];
}

bool input_eclat(const Input& in)
{
    return action_pressed(in, ACT_ECLAT) || in.pad_pressed[PAD_B];
}

bool input_dash(const Input& in)
{
    return action_pressed(in, ACT_DASH) || in.pad_pressed[PAD_LB];
}

bool input_switch(const Input& in)
{
    return action_pressed(in, ACT_SWITCH) || in.pad_pressed[PAD_Y] || in.pad_pressed[PAD_RB];
}

bool input_confirm(const Input& in)
{
    return in.pressed[KEY_ENTER] || in.pressed[KEY_SPACE] || action_pressed(in, ACT_JUMP) ||
           in.pad_pressed[PAD_A] || in.pad_pressed[PAD_START];
}

bool input_back(const Input& in)
{
    return action_pressed(in, ACT_PAUSE) || in.pad_pressed[PAD_START];
}

// skip a whole dialogue or cutscene: the pause keys do it
bool input_skip(const Input& in)
{
    return action_pressed(in, ACT_PAUSE) || in.pad_pressed[PAD_BACK];
}

static void player_read(World& w, Entity e, const Json& j)
{
    Player& p = w.has<Player>(e) ? *w.get<Player>(e) : w.add<Player>(e);
    if (j["facing"].is_num())
        p.facing = j["facing"].i() < 0 ? -1 : 1;
}

static Json player_write(World& w, Entity e)
{
    Json j = Json::object();
    j.set("facing", Json::number(w.get<Player>(e)->facing));
    return j;
}

static void player_inspect(World& w, Entity e)
{
    Player& p = *w.get<Player>(e);
    ui_drag_int("facing", &p.facing);
    ui_label("coyote %.2f  buffer %.2f", p.coyote, p.buffer);
}

void register_game_components()
{
    component_register(component_type_for<Player>("Player", player_read, player_write, player_inspect));
}

bool player_alive()
{
    return G.player && G.scene.world.alive(G.player);
}

static float approach(float v, float target, float step)
{
    if (v < target)
        return fminf(v + step, target);
    return fmaxf(v - step, target);
}

void player_update(const Input& in, float dt)
{
    if (!player_alive())
        return;
    World& w = G.scene.world;
    Entity e = G.player;
    Player& p = *w.get<Player>(e);
    Body& b = *w.get<Body>(e);
    Transform& t = *w.get<Transform>(e);
    Collider& c = *w.get<Collider>(e);
    Tuning k = world_tuning();

    int dir = (input_right(in) ? 1 : 0) - (input_left(in) ? 1 : 0);
    bool ground = b.on_ground;
    if (p.aiming && ground) {
        if (dir != 0)
            p.facing = dir;
        dir = 0;
    }

    // landing
    if (ground && !p.was_ground) {
        if (p.fall_speed > 140) {
            audio_play(sfx("land"), fminf(1, p.fall_speed / 400), randf(0.9f, 1.1f));
            particles_burst(particle_fx("dust"), t.pos, 5);
            p.squash = fminf(0.35f, p.fall_speed / 900);
        }
        p.jumping = false;
    }
    if (!ground)
        p.fall_speed = b.vel.y;
    p.was_ground = ground;

    p.coyote = ground ? k.coyote : p.coyote - dt;
    p.buffer = input_jump_pressed(in) ? k.buffer : p.buffer - dt;
    p.lock -= dt;

    // walls: probe a pixel to each side, the physics flag only says we pushed into one
    Rect r = collider_rect(t, c);
    int wall = 0;
    if (!ground) {
        if (rect_hits_solid(G.scene.map, G.tileset, { r.x - 1, r.y + 2, 1, r.h - 4 })) wall = -1;
        if (rect_hits_solid(G.scene.map, G.tileset, { r.x + r.w, r.y + 2, 1, r.h - 4 })) wall = 1;
    }
    if (wall) {
        p.wall_dir = wall;
        p.wall_grace = 0.08f;
    } else {
        p.wall_grace -= dt;
    }

    // run
    // wind adds to whatever speed she's going for, and still gets its way
    // when she lets go
    float target = dir * k.run + p.wind.x;
    float accel;
    if (dir != 0)
        accel = ground ? k.accel : k.air_accel;
    else
        accel = ground ? k.friction : (p.wind.x != 0 ? k.air_accel * 0.6f : k.air_friction);
    if (p.lock > 0)
        accel *= 0.3f;
    if (ground && (rect_tile_flags(G.scene.map, G.tileset, { r.x, r.y + r.h, r.w, 2 }) & TILE_ICE))
        accel *= dir != 0 ? 0.25f : 0.08f;
    b.vel.x = approach(b.vel.x, target, accel * dt);
    if (dir != 0 && p.lock <= 0)
        p.facing = dir;

    // wall slide, only while pushing into the wall
    bool sliding = wall != 0 && dir == wall && b.vel.y > 0;
    if (sliding) {
        b.vel.y = fminf(b.vel.y, k.wall_slide);
        if (randf(0, 1) < 0.3f)
            particles_burst(particle_fx("wall_dust"), { t.pos.x + wall * 4.f, t.pos.y - 6 }, 1);
    }

    // drop through one-way ledges with down + jump
    if (ground && input_down(in) && p.buffer > 0) {
        p.drop = 0.2f;
        p.buffer = 0;
    }
    p.drop -= dt;
    b.drop_through = p.drop > 0;

    // jump
    if (p.buffer > 0 && p.coyote > 0) {
        b.vel.y = -k.jump;
        p.buffer = p.coyote = 0;
        p.jumping = true;
        p.squash = -0.3f;
        audio_play(sfx("jump"), 0.6f, randf(0.95f, 1.05f));
        particles_burst(particle_fx("dust"), t.pos, 4);
    } else if (p.buffer > 0 && p.wall_grace > 0) {
        b.vel.x = -p.wall_dir * k.wall_jump_x;
        b.vel.y = -k.wall_jump_y;
        p.facing = -p.wall_dir;
        p.buffer = 0;
        p.wall_grace = 0;
        p.lock = 0.16f;
        p.jumping = true;
        p.squash = -0.25f;
        audio_play(sfx("jump"), 0.6f, 1.15f);
        particles_burst(particle_fx("dust"), { t.pos.x + p.wall_dir * 4.f, t.pos.y - 5 }, 4);
    }

    // let go early for a short hop
    if (p.jumping && !input_jump_held(in) && b.vel.y < 0) {
        b.vel.y *= k.jump_cut;
        p.jumping = false;
    }

    // heavier on the way down, floaty at the top of the arc if you keep holding
    if (b.vel.y > 0)
        b.gravity = k.fall_gravity;
    else if (fabsf(b.vel.y) < 40 && input_jump_held(in))
        b.gravity = k.apex_gravity;
    else
        b.gravity = 1;

    // rising air: the jump is over, the updraft takes it from here
    if (p.wind.y < 0) {
        b.vel.y = fmaxf(b.vel.y + p.wind.y * dt, -210);
        p.jumping = false;
    }
    p.wind = {};

    // spread out and drift down while holding jump
    p.gliding = has_power("planer") && !ground && !sliding && b.vel.y > 0 && input_jump_held(in);
    if (p.gliding) {
        float fall_speed = has_bonus("foret") ? 28.0f : 42.0f;
        if (relic_is_equipped("plume_suie"))
            fall_speed *= 0.72f;
        b.vel.y = fminf(b.vel.y, fall_speed);
        b.gravity = 0.3f;
        p.squash = fmaxf(p.squash, 0.18f);
    }

    // the forge's gift: a straight burst of fire, once per jump (or twice with silex_vivace)
    if (ground) {
        p.dash_ready = true;
        p.air_dashes = 0;
    }
    p.dash_cool -= dt;
    int max_air = relic_is_equipped("silex_vivace") ? 2 : 1;
    bool can_dash = ground || p.air_dashes < max_air;
    if (has_power("elan") && input_dash(in) && can_dash && p.dash_cool <= 0 && p.dash <= 0) {
        p.dash = DASH_TIME * (has_bonus("glace") ? 1.35f : 1);
        p.dash_cool = relic_is_equipped("silex_vivace") ? 0.22f : 0.35f;
        if (!ground)
            p.air_dashes++;
        p.dash_ready = ground;
        p.dashed.clear();
        p.jumping = false;
        audio_play(sfx("dash"), 0.6f, randf(0.95f, 1.05f));
        particles_burst(particle_fx("flame_burst"), t.pos - vec2{ 0, 6 });
        add_shake(1);
    }
    if (p.dash > 0) {
        p.dash -= dt;
        b.vel = { p.facing * DASH_SPEED, 0 };
        b.gravity = 0;
        p.gliding = false;
        p.squash = 0.25f;
        particles_burst(particle_fx("flame"), t.pos - vec2{ p.facing * 4.f, 6 }, 2);
    }

    // footsteps
    if (ground && fabsf(b.vel.x) > 30) {
        p.step -= dt;
        if (p.step <= 0) {
            p.step = 0.24f;
            audio_play(sfx("step"), 0.25f, randf(0.85f, 1.15f));
            particles_burst(particle_fx("dust"), t.pos, 1);
        }
    }

    p.squash = approach(p.squash, 0, dt * 1.6f);
    float s = p.squash;
    t.scale = { 1 + s, 1 - s };

    Sprite& spr = *w.get<Sprite>(e);
    spr.flip_x = p.facing < 0;
    if (sliding)
        anim_play(w, e, "wall");
    else if (!ground)
        anim_play(w, e, b.vel.y < 0 ? "jump" : "fall");
    else if (fabsf(b.vel.x) > 12)
        anim_play(w, e, "run");
    else
        anim_play(w, e, "idle");

    // tiles that hurt, water that puts her out, lava that throws her back up,
    // and falling out of the map
    Rect hurt = { r.x + 1, r.y + 2, r.w - 2, r.h - 3 };
    uint8_t touching = rect_tile_flags(G.scene.map, G.tileset, hurt);
    if (touching & TILE_WATER) {
        kill_player("water");
        return;
    }
    if ((touching & TILE_HAZARD) || t.pos.y > G.scene.map.h * G.tileset.size + 32) {
        kill_player();
        return;
    }
    if ((touching & TILE_LAVA) && b.vel.y >= 0) {
        // she's a flame, lava doesn't hurt her, it launches her
        b.vel.y = -k.jump * 1.25f;
        p.jumping = false;
        p.squash = -0.35f;
        particles_burst(particle_fx("lava_splash"), t.pos);
        audio_play(sfx("lava"), 0.6f, randf(0.9f, 1.1f));
    }

    player_combat_update(in, dt, ground);
}
