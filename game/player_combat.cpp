// Player combat, weapons (spear, bow, arrows), powers (elan, eclat), and light queries.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "game.h"
#include "relics.h"

Rect spear_rect(const Transform& t, const Player& p)
{
    if (p.attack_down)
        return { t.pos.x - 4, t.pos.y - 2, 8, 18 };
    if (p.attack_up)
        return { t.pos.x - 4, t.pos.y - 36, 8, 24 };
    float x0 = p.facing > 0 ? t.pos.x + 2 : t.pos.x - 24;
    return { x0, t.pos.y - 10, 22, 7 };
}

void spear_hits(Entity self, Player& p, Transform& t)
{
    World& w = G.scene.world;
    Rect r = spear_rect(t, p);
    static std::vector<Entity> found;
    found.clear();
    query_rect(w, r, L_ENEMY | L_WORLD, found);

    vec2 dir = { (float)p.facing, 0 };
    if (p.attack_down)
        dir = { 0, 1 };
    else if (p.attack_up)
        dir = { 0, -1 };
    bool landed = false;
    vec2 where = { r.x + r.w / 2, r.y + r.h / 2 };
    for (Entity e : found) {
        if (e == self || std::find(p.struck.begin(), p.struck.end(), e) != p.struck.end())
            continue;
        p.struck.push_back(e);
        Value took;
        if (w.has<Script>(e)) {
            if (script_send(w, e, "hit", { Value::number(dir.x), Value::number(dir.y) }, &took) && took.truthy())
                landed = true;
        } else if (Body* b = w.get<Body>(e)) {
            if (b->type == BODY_DYNAMIC) {
                b->vel += dir * 150;
                landed = true;
            }
        }
        if (landed)
            where = w.get<Transform>(e)->pos - vec2{ 0, 5 };
    }

    if (!landed)
        return;
    engine_hitstop(0.05f);
    add_shake(1.5f);
    audio_play(sfx("hit"), 0.7f, randf(0.9f, 1.1f));
    particles_burst(particle_fx("spark"), where);
    if (p.attack_down) {
        float b_vy = relic_is_equipped("lame_suie") ? -330.0f : -265.0f;
        bounce_player(b_vy);
        p.attack = 0;
    }
}

bool lit_at(vec2 p)
{
    World& w = G.scene.world;
    if (G.eclat_glow > 0) {
        vec2 d = p - G.eclat_pos;
        float r = ECLAT_RADIUS * fminf(1, G.eclat_glow * 1.5f);
        if (d.x * d.x + d.y * d.y < r * r)
            return true;
    }
    for (const Arrow& a : G.arrows) {
        vec2 d = p - a.pos;
        float r = a.stuck ? 22 * a.life : 30;
        if (d.x * d.x + d.y * d.y < r * r)
            return true;
    }
    return light_reaches(w, p);
}

void add_arrows(int n)
{
    if (G.has[WEAPON_BOW])
        G.quiver = std::min(quiver_max(), G.quiver + n);
}

bool has_power(const char* name)
{
    return std::find(G.powers.begin(), G.powers.end(), name) != G.powers.end();
}

bool give_weapon(const std::string& name)
{
    if (name == "planer" || name == "feu" || name == "elan" || name == "eclat") {
        if (!has_power(name.c_str()))
            G.powers.push_back(name);
        if (name == "planer")
            say("pouvoir : planer. Maintiens le saut en tombant", 3.5f);
        else if (name == "feu")
            say("pouvoir : flèches de feu. Elles allument les feux et font fondre la glace", 4);
        else if (name == "elan")
            say("pouvoir : l'élan. {elan} : une ruée de feu qui brise ce qui est fragile", 4);
        else
            say("pouvoir : l'éclat. {eclat} : une explosion de lumière qui chasse les ombres", 4);
    } else if (name == "lance") {
        G.has[WEAPON_SPEAR] = true;
        G.weapon = WEAPON_SPEAR;
        say("lance obtenue : {arme} pour frapper", 3);
    } else if (name == "arc") {
        G.has[WEAPON_BOW] = true;
        G.weapon = WEAPON_BOW;
        G.quiver = std::max(G.quiver, 6);
        say("arc obtenu : {changer} pour changer d'arme. Maintiens {arme} pour viser (haut/bas), relâche pour tirer", 4.5f);
    } else {
        fprintf(stderr, "no weapon called %s\n", name.c_str());
        return false;
    }
    audio_play(sfx("quiver"), 0.7f);
    return true;
}

static int level_index_by_name(const char* name)
{
    for (size_t i = 0; i < G.levels.size(); i++) {
        if (G.levels[i] == name)
            return (int)i;
    }
    return -1;
}

void weapons_for_level(int level)
{
    int hollow_idx = level_index_by_name("hollow");
    int roots_idx = level_index_by_name("roots");
    if (level > (hollow_idx >= 0 ? hollow_idx : 0))
        G.has[WEAPON_SPEAR] = true;
    if (level > (roots_idx >= 0 ? roots_idx : 1))
        G.has[WEAPON_BOW] = true;
    struct { const char* world; const char* power; bool after; } gifts[] = {
        { "ciel", "planer", false },
        { "froid", "feu", true },
        { "forge", "elan", true },
        { "ombres", "eclat", false },
    };
    for (const auto& g : gifts) {
        int first = 0;
        size_t i = 0;
        for (; i < G.worlds.size() && G.worlds[i].id != g.world; i++)
            first += (int)G.worlds[i].levels.size();
        if (i == G.worlds.size())
            continue;
        int from = g.after ? first + (int)G.worlds[i].levels.size() : first;
        if (level >= from && !has_power(g.power))
            G.powers.push_back(g.power);
    }
    if (!G.has[G.weapon])
        G.weapon = WEAPON_SPEAR;
}

void arrows_update(float dt)
{
    World& w = G.scene.world;
    static std::vector<Entity> found;
    float bottom = (float)(G.scene.map.h * G.tileset.size + 32);
    for (size_t i = 0; i < G.arrows.size();) {
        Arrow& a = G.arrows[i];
        a.prev = a.pos;
        a.life -= dt;
        bool gone = a.life <= 0 || a.pos.y > bottom;
        if (!a.stuck && !gone) {
            a.vel.y += ARROW_GRAVITY * dt;
            a.pos += a.vel * dt;
            a.angle = atan2f(a.vel.y, a.vel.x);
            vec2 dir = { cosf(a.angle), sinf(a.angle) };
            vec2 tip = a.pos + dir * 5;
            Rect r = { tip.x - 2, tip.y - 2, 4, 4 };

            bool thermal_arrow = has_power("feu") || relic_is_equipped("souffle_chaud");
            found.clear();
            query_rect(w, r, L_ENEMY | L_WORLD | (thermal_arrow ? L_PICKUP : 0), found);
            for (Entity e : found) {
                Value took;
                if (w.has<Script>(e) && script_send(w, e, "hit", { Value::number(dir.x), Value::number(dir.y) }, &took) &&
                    took.truthy()) {
                    audio_play(sfx("hit"), 0.6f, randf(1.0f, 1.2f));
                    particles_burst(particle_fx("spark"), tip);
                    engine_hitstop(0.03f);
                    gone = true;
                    break;
                }
            }
            if (!gone && thermal_arrow && randf(0, 1) < 0.5f)
                particles_burst(particle_fx("flame"), a.pos, 1);
            if (!gone && rect_hits_solid(G.scene.map, G.tileset, r)) {
                a.stuck = true;
                a.life = 1.2f;
                audio_play(sfx("land"), 0.3f, 1.8f);
            }
        }
        if (gone) {
            G.arrows[i] = G.arrows.back();
            G.arrows.pop_back();
        } else {
            i++;
        }
    }
}

void dash_hits(Entity self, Player& p, Transform& t)
{
    World& w = G.scene.world;
    Rect r = { t.pos.x - 6 + p.facing * 6.f, t.pos.y - 12, 12, 11 };
    static std::vector<Entity> found;
    found.clear();
    query_rect(w, r, L_ENEMY | L_WORLD, found);
    for (Entity e : found) {
        if (e == self || !w.has<Script>(e) || std::find(p.dashed.begin(), p.dashed.end(), e) != p.dashed.end())
            continue;
        p.dashed.push_back(e);
        Value took;
        Value dir[] = { Value::number((float)p.facing), Value::number(0) };
        if (script_send(w, e, "dashed", { dir[0], dir[1] }, &took) && took.truthy())
            continue;
        if (script_send(w, e, "hit", { dir[0], dir[1] }, &took) && took.truthy()) {
            audio_play(sfx("hit"), 0.6f, 1.2f);
            particles_burst(particle_fx("spark"), w.get<Transform>(e)->pos);
        }
    }
}

void player_combat_update(const Input& in, float dt, bool ground)
{
    if (!player_alive())
        return;
    World& w = G.scene.world;
    Entity e = G.player;
    Player& p = *w.get<Player>(e);
    Transform& t = *w.get<Transform>(e);

    if (input_switch(in) && G.has[WEAPON_SPEAR] && G.has[WEAPON_BOW]) {
        G.weapon = G.weapon == WEAPON_SPEAR ? WEAPON_BOW : WEAPON_SPEAR;
        p.aiming = false;
        audio_play(sfx("menu"), 0.4f, G.weapon == WEAPON_BOW ? 1.2f : 0.9f);
    }
    bool spear = G.weapon == WEAPON_SPEAR && G.has[WEAPON_SPEAR];
    bool bow = G.weapon == WEAPON_BOW && G.has[WEAPON_BOW];

    p.attack_cool -= dt;
    if (spear && input_attack(in) && p.attack_cool <= 0) {
        p.attack = SPEAR_TIME;
        p.attack_cool = 0.32f;
        p.attack_down = !ground && input_down(in);
        p.attack_up = !p.attack_down && input_up(in);
        p.struck.clear();
        audio_play(sfx("spear"), 0.5f, randf(0.95f, 1.1f));
        if (p.attack_up)
            p.squash = -0.15f;
        else if (!p.attack_down)
            p.squash = 0.12f;
    }

    p.bow_cool -= dt;
    if (!bow) {
        p.aiming = false;
    } else if (input_attack(in) && p.bow_cool <= 0 && !p.aiming) {
        if (G.quiver > 0) {
            p.aiming = true;
            p.aim = input_up(in) ? -0.95f : 0;
        } else {
            audio_play(sfx("menu"), 0.3f, 0.6f);
            say("plus de flèches", 1.2f);
        }
    } else if (p.aiming) {
        int turn = (input_down(in) ? 1 : 0) - (input_up(in) ? 1 : 0);
        p.aim = fminf(0.8f, fmaxf(-1.45f, p.aim + turn * 1.4f * dt));
        if (!input_attack_held(in)) {
            p.aiming = false;
            p.bow_cool = 0.25f;
            G.quiver--;
            Arrow a;
            a.pos = a.prev = t.pos + vec2{ p.facing * 5.f, -7 };
            a.vel = vec2{ p.facing * cosf(p.aim), sinf(p.aim) } * ARROW_SPEED;
            a.life = 2.5f;
            G.arrows.push_back(a);
            audio_play(sfx("bow"), 0.5f, randf(0.95f, 1.05f));
        }
    }
    if (p.attack > 0) {
        p.attack -= dt;
        spear_hits(e, p, t);
    }
    if (p.dash > 0)
        dash_hits(e, p, t);

    G.eclat_cool -= dt;
    if (has_power("eclat") && input_eclat(in) && G.eclat_cool <= 0 && player_alive()) {
        G.eclat_cool = ECLAT_COOL * (has_bonus("mer") ? 0.7f : 1);
        G.eclat_glow = 1;
        G.eclat_pos = w.get<Transform>(e)->pos - vec2{ 0, 6 };
        audio_play(sfx("eclat"), 0.8f);
        particles_burst(particle_fx("eclat"), G.eclat_pos);
        add_shake(2);
        engine_hitstop(0.04f);
        static std::vector<Entity> found;
        found.clear();
        vec2 c = G.eclat_pos;
        query_rect(w, { c.x - ECLAT_RADIUS, c.y - ECLAT_RADIUS, ECLAT_RADIUS * 2, ECLAT_RADIUS * 2 },
                   L_ENEMY | L_WORLD | L_PICKUP, found);
        for (Entity o : found) {
            if (o == e || !w.alive(o) || !w.has<Script>(o))
                continue;
            vec2 d = w.get<Transform>(o)->pos - c;
            if (d.x * d.x + d.y * d.y < ECLAT_RADIUS * ECLAT_RADIUS)
                script_send(w, o, "eclat", {});
        }
    }
}

