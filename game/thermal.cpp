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
        // Ease off near the top of the column so Lueur hovers smoothly instead of bouncing
        float dist_from_top = pos.y - (v.pos.y - v.height);
        float factor = clampf(dist_from_top / 28.0f, 0.25f, 1.0f);
        // Scale scene power (~380-420) into upward acceleration that overcomes gravity (900)
        float base_accel = v.power * 3.8f;
        float multiplier = gliding ? 1.45f : 1.0f;
        if (relic_is_equipped("plume_suie"))
            multiplier *= 1.35f;
        return base_accel * factor * multiplier;
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

    // Feed upward acceleration into player_update's wind channel for smooth ascent
    p->wind.y -= lift;
    if (p->gliding) {
        p->squash = fmaxf(p->squash, 0.25f);
        if (randf(0, 1) < 0.4f)
            particles_burst(particle_fx("flame"), t->pos - vec2{ 0, 4 }, 1);
    } else if (b->vel.y > 40.0f) {
        b->vel.y = 40.0f;
    }
}
