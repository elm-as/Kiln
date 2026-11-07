#include "camera.h"
#include <cmath>
#include <cstdlib>

// shake doesn't need the engine's random numbers, and this keeps the file
// usable on its own (the tests link it without the engine)
static float jitter(float r)
{
    return (rand() / (float)RAND_MAX * 2 - 1) * r;
}

static float clamp_axis(float v, float lo, float len, float view)
{
    if (len <= 0)
        return v;
    // a level smaller than the view sits in the middle of it
    if (len < view)
        return lo + (len - view) / 2;
    return clampf(v, lo, lo + len - view);
}

vec2 camera_target(const Camera& c, vec2 focus)
{
    // leaning towards whatever it's asked to look at, never so far that the
    // focus leaves the screen
    if (c.looking) {
        vec2 d = (c.look_at - focus) * c.look_weight;
        d.x = clampf(d.x, -c.size.x / 2 + 48, c.size.x / 2 - 48);
        d.y = clampf(d.y, -c.size.y / 2 + 40, c.size.y / 2 - 40);
        focus += d;
    }
    vec2 p = focus - c.size * 0.5f;
    p.x = clamp_axis(p.x, c.bounds.x, c.bounds.w, c.size.x);
    p.y = clamp_axis(p.y, c.bounds.y, c.bounds.h, c.size.y);
    return p;
}

void camera_snap(Camera& c, vec2 focus)
{
    c.pos = c.prev = camera_target(c, focus);
}

void camera_update(Camera& c, vec2 focus, float dt)
{
    c.prev = c.pos;
    vec2 target = camera_target(c, focus);
    c.looking = false;
    c.pos.x += (target.x - c.pos.x) * (1 - expf(-dt * c.follow_x));
    c.pos.y += (target.y - c.pos.y) * (1 - expf(-dt * c.follow_y));
    c.shake = fmaxf(0, c.shake - dt * c.shake_decay);
}

void camera_shake(Camera& c, float amount)
{
    if (amount > c.shake)
        c.shake = amount;
}

void camera_look(Camera& c, vec2 at)
{
    c.look_at = at;
    c.looking = true;
}

vec2 camera_view(const Camera& c, float alpha)
{
    vec2 p = { lerp(c.prev.x, c.pos.x, alpha), lerp(c.prev.y, c.pos.y, alpha) };
    if (c.shake > 0)
        p += vec2{ jitter(c.shake), jitter(c.shake) } * 0.5f;
    return { floorf(p.x + 0.5f), floorf(p.y + 0.5f) };
}
