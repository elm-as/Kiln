#include "thermal.h"
#include <algorithm>
#include <cmath>
#include "game.h"
#include "relics.h"

static std::vector<SteamVent> vents;

void thermal_init()
{
    vents.clear();
}

void thermal_reset()
{
    vents.clear();
}

int thermal_add_vent(vec2 pos, float w, float h, float power, bool active)
{
    SteamVent v;
    v.pos = pos;
    v.width = w;
    v.height = h;
    v.power = power;
    v.active = active;
    vents.push_back(v);
    return (int)vents.size() - 1;
}

void thermal_set_vent_active(int id, bool active)
{
    if (id < 0 || id >= (int)vents.size())
        return;
    vents[id].active = active;
}

static bool is_in_vent(const SteamVent& v, vec2 p)
{
    if (!v.active)
        return false;
    float half_w = v.width * 0.5f;
    if (p.x < v.pos.x - half_w || p.x > v.pos.x + half_w)
        return false;
    if (p.y > v.pos.y + 4.0f || p.y < v.pos.y - v.height)
        return false;
    return true;
}

float thermal_vent_lift_at(vec2 pos, bool gliding)
{
    for (const SteamVent& v : vents) {
        if (!is_in_vent(v, pos))
            continue;
        // Near the top of the column, ease off so the player floats
        float dist_from_top = pos.y - (v.pos.y - v.height);
        float factor = clampf(dist_from_top / 24.0f, 0.2f, 1.0f);
        float multiplier = gliding ? 1.4f : 0.6f;
        if (relic_is_equipped("plume_suie"))
            multiplier *= 1.35f;
        return v.power * factor * multiplier;
    }
    return 0.0f;
}

void thermal_update(float dt)
{
    (void)dt;
    if (vents.empty())
        return;

    // Emit ambient steam particles from active vents
    for (const SteamVent& v : vents) {
        if (!v.active)
            continue;

        if (randf(0, 1) < 0.35f) {
            float rx = v.pos.x + randf(-v.width * 0.45f, v.width * 0.45f);
            float ry = v.pos.y - randf(0, v.height * 0.25f);
            particles_burst(particle_fx("steam"), { rx, ry }, 1);
        }
        if (randf(0, 1) < 0.25f) {
            float rx = v.pos.x + randf(-v.width * 0.4f, v.width * 0.4f);
            float ry = v.pos.y - randf(0, v.height);
            particles_burst(particle_fx("wind_up"), { rx, ry }, 1);
        }
    }

    if (!player_alive())
        return;

    World& w = G.scene.world;
    Transform* t = w.get<Transform>(G.player);
    Body* b = w.get<Body>(G.player);
    Player* p = w.get<Player>(G.player);
    if (!t || !b || !p)
        return;

    float lift = thermal_vent_lift_at(t->pos, p->gliding);
    if (lift <= 0.001f)
        return;

    if (p->gliding) {
        // Strong vertical propulsion when wings/glide are spread
        b->vel.y = fminf(b->vel.y, -lift);
        p->wind.y -= lift * 1.6f;
        p->squash = fmaxf(p->squash, 0.3f);
        if (randf(0, 1) < 0.4f) {
            particles_burst(particle_fx("flame"), t->pos - vec2{ 0, 4 }, 1);
        }
    } else {
        // Cushion fall or mild updraft while not gliding
        b->vel.y = fminf(b->vel.y, 50.0f);
        p->wind.y -= lift * 0.4f;
    }
}
