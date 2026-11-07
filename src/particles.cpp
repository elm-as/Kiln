#include "particles.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

struct Fx {
    std::string name;
    int count = 8;
    float life0 = 0.5f, life1 = 1;
    float speed0 = 20, speed1 = 60;
    float angle = -90, spread = 360;
    float gravity = 0;
    float drag = 0;
    float size0 = 2, size1 = 0;
    Color color0 = WHITE, color1 = { 255, 255, 255, 0 };
    bool additive = false;
    float rate = 10;
    vec2 area;
};

struct Particle {
    vec2 pos, vel;
    float life, max_life;
    int fx;
};

enum { MAX_PARTICLES = 4096 };

static std::vector<Fx> effects;
static std::map<std::string, int> fx_ids;
static Particle parts[MAX_PARTICLES];
static int count;
static uint32_t rng = 0x2545f491;

static float frand(float lo, float hi)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return lo + (hi - lo) * ((rng >> 8) * (1.0f / 16777216.0f));
}

static Color parse_color(const Json& j, Color def)
{
    if (!j.is_str())
        return def;
    const char* s = j.str();
    if (*s == '#')
        s++;
    unsigned long v = strtoul(s, 0, 16);
    size_t n = strlen(s);
    if (n == 6)
        return rgb((uint32_t)v);
    if (n == 8)
        return { uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v) };
    return def;
}

static void range(const Json& j, float* a, float* b)
{
    if (j.is_num()) {
        *a = *b = j.f();
    } else if (j.size() == 2) {
        *a = j[0].f();
        *b = j[1].f();
    }
}

void particles_load(const Json& j)
{
    // ids stay put across reloads, new names get appended
    for (const auto& kv : j.obj) {
        const Json& d = kv.second;
        Fx f;
        f.name = kv.first;
        f.count = d["count"].i(f.count);
        range(d["life"], &f.life0, &f.life1);
        range(d["speed"], &f.speed0, &f.speed1);
        f.angle = d["angle"].f(f.angle);
        f.spread = d["spread"].f(f.spread);
        f.gravity = d["gravity"].f(0);
        f.drag = d["drag"].f(0);
        range(d["size"], &f.size0, &f.size1);
        if (d["color"].is_arr()) {
            f.color0 = parse_color(d["color"][0], f.color0);
            f.color1 = parse_color(d["color"][1], fade(f.color0, 0));
        } else if (d["color"].is_str()) {
            f.color0 = parse_color(d["color"], f.color0);
            f.color1 = fade(f.color0, 0);
        }
        f.additive = d["additive"].boolean_or(false);
        f.rate = d["rate"].f(f.rate);
        if (d["area"].size() == 2)
            f.area = { d["area"][0].f(), d["area"][1].f() };

        auto it = fx_ids.find(f.name);
        if (it != fx_ids.end()) {
            effects[it->second] = f;
        } else {
            fx_ids[f.name] = (int)effects.size();
            effects.push_back(f);
        }
    }
}

int particle_fx(const char* name)
{
    auto it = fx_ids.find(name);
    return it == fx_ids.end() ? -1 : it->second;
}

const char* particle_fx_name(int fx)
{
    if (fx < 0 || fx >= (int)effects.size())
        return "";
    return effects[fx].name.c_str();
}

static void spawn(int fx, vec2 pos, float angle_deg)
{
    if (count >= MAX_PARTICLES)
        return;
    const Fx& f = effects[fx];
    Particle& p = parts[count++];
    p.pos = pos + vec2{ frand(-f.area.x, f.area.x), frand(-f.area.y, f.area.y) };
    float a = (angle_deg + frand(-f.spread, f.spread) * 0.5f) * 0.0174533f;
    float sp = frand(f.speed0, f.speed1);
    p.vel = { cosf(a) * sp, sinf(a) * sp };
    p.max_life = p.life = frand(f.life0, f.life1);
    p.fx = fx;
}

void particles_burst(int fx, vec2 pos, int n, float angle)
{
    if (fx < 0 || fx >= (int)effects.size())
        return;
    const Fx& f = effects[fx];
    if (n < 0)
        n = f.count;
    float a = angle > 1e8f ? f.angle : angle;
    for (int i = 0; i < n; i++)
        spawn(fx, pos, a);
}

void particles_update(World& w, float dt)
{
    w.each<Emitter, Transform>([&](Entity, Emitter& em, Transform& t) {
        if (!em.on || em.fx < 0 || em.fx >= (int)effects.size())
            return;
        em.acc += effects[em.fx].rate * em.rate * dt;
        while (em.acc >= 1) {
            em.acc -= 1;
            spawn(em.fx, t.pos + em.offset, effects[em.fx].angle);
        }
    });

    for (int i = 0; i < count;) {
        Particle& p = parts[i];
        p.life -= dt;
        if (p.life <= 0) {
            p = parts[--count];
            continue;
        }
        const Fx& f = effects[p.fx];
        p.vel.y += f.gravity * dt;
        if (f.drag > 0)
            p.vel = p.vel * (1 / (1 + f.drag * dt));
        p.pos += p.vel * dt;
        i++;
    }
}

void particles_draw(bool additive)
{
    for (int i = 0; i < count; i++) {
        const Particle& p = parts[i];
        const Fx& f = effects[p.fx];
        if (f.additive != additive)
            continue;
        float k = 1 - p.life / p.max_life;
        float size = f.size0 + (f.size1 - f.size0) * k;
        if (size <= 0.2f)
            continue;
        Color c = mix(f.color0, f.color1, k);
        // snap to whole pixels, sub pixel squares shimmer on the pixel grid
        float s = fmaxf(1, floorf(size + 0.5f));
        draw_rect({ floorf(p.pos.x - s / 2 + 0.5f), floorf(p.pos.y - s / 2 + 0.5f), s, s }, c);
    }
}

void particles_clear()
{
    count = 0;
}

int particles_alive()
{
    return count;
}
