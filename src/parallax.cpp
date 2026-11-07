#include "parallax.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "assets.h"

static Color hex_color(const Json& j, Color def)
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

std::vector<ParallaxLayer> parallax_load(const Json& layers)
{
    std::vector<ParallaxLayer> out;
    for (const Json& j : layers.arr) {
        ParallaxLayer l;
        l.sprite = sprite_id(j["sprite"].str());
        if (l.sprite < 0)
            continue;
        l.frame = j["frame"].i(0);
        l.speed = j["speed"].f(0.5f);
        l.speed_y = j["speed_y"].f(-1);
        l.top = !strcmp(j["anchor"].str("bottom"), "top");
        l.offset = j["offset"].f(0);
        l.gap = j["gap"].f(0);
        l.drift = j["drift"].f(0);
        l.front = j["front"].boolean_or(false);
        l.tint = hex_color(j["tint"], WHITE);
        out.push_back(l);
    }
    return out;
}

void parallax_draw(const std::vector<ParallaxLayer>& layers, bool front, vec2 cam, vec2 view, float world_h, double time)
{
    for (const ParallaxLayer& l : layers) {
        if (l.front != front)
            continue;
        const SpriteSheet* s = sprite_get(l.sprite);
        if (!s)
            continue;
        float w = (float)s->w, h = (float)s->h;
        float period = w + l.gap;

        // how far the camera is from the middle of the level, vertically.
        // Layers only slide a little with it, and only away from their edge:
        // a ceiling can rise out of view but never come down into the middle
        // of a tall level, a floor can sink but never float. Front layers
        // don't move vertically at all unless asked.
        float sy = l.speed_y >= 0 ? l.speed_y : (l.front ? 0 : l.speed * 0.35f);
        float mid = (world_h - view.y) * 0.5f;
        float dy = -(cam.y - mid) * sy;
        dy = l.top ? fminf(dy, 0) : fmaxf(dy, 0);

        float y = l.top ? l.offset : view.y - h - l.offset;
        y = floorf(y + dy + 0.5f);

        double scroll = cam.x * l.speed + time * l.drift;
        float start = -(float)fmod(scroll, (double)period);
        if (start > 0)
            start -= period;

        SpriteOpts o;
        o.tint = l.tint;
        for (float x = floorf(start); x < view.x; x += period)
            draw_spr_ex(l.sprite, l.frame, { x, y }, o);
    }
}
