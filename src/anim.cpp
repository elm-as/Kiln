#include "anim.h"
#include <cmath>
#include <vector>
#include "assets.h"

void anim_play(World& w, Entity e, const char* name, bool restart)
{
    Sprite* s = w.get<Sprite>(e);
    if (!s)
        return;
    const SpriteSheet* sheet = sprite_get(s->sprite);
    if (!sheet)
        return;
    int a = sheet->anim(name);
    if (a < 0)
        return;
    Animator* an = w.get<Animator>(e);
    if (!an)
        an = &w.add<Animator>(e);
    if (an->anim == a && !restart)
        return;
    an->anim = a;
    an->step = 0;
    an->time = 0;
    an->done = false;
    if (!sheet->anims[a].frames.empty())
        s->frame = sheet->anims[a].frames[0];
}

bool anim_is(World& w, Entity e, const char* name)
{
    Sprite* s = w.get<Sprite>(e);
    Animator* an = w.get<Animator>(e);
    if (!s || !an)
        return false;
    const SpriteSheet* sheet = sprite_get(s->sprite);
    return sheet && an->anim >= 0 && an->anim < (int)sheet->anims.size() && sheet->anims[an->anim].name == name;
}

void anim_update(World& w, float dt)
{
    w.each<Animator, Sprite>([&](Entity, Animator& an, Sprite& s) {
        if (s.flash > 0)
            s.flash = fmaxf(0, s.flash - dt * 6);

        const SpriteSheet* sheet = sprite_get(s.sprite);
        if (!sheet || an.anim < 0 || an.anim >= (int)sheet->anims.size() || an.done)
            return;
        const Anim& a = sheet->anims[an.anim];
        if (a.frames.empty() || a.fps <= 0)
            return;

        an.time += dt * an.speed;
        float frame_time = 1 / a.fps;
        while (an.time >= frame_time) {
            an.time -= frame_time;
            an.step++;
            if (an.step >= (int)a.frames.size()) {
                if (a.loop) {
                    an.step = 0;
                } else {
                    an.step = (int)a.frames.size() - 1;
                    an.done = true;
                    break;
                }
            }
        }
        s.frame = a.frames[an.step];
    });

    // sprites without an animator still want their flash to fade
    w.each<Sprite>([&](Entity e, Sprite& s) {
        if (s.flash > 0 && !w.has<Animator>(e))
            s.flash = fmaxf(0, s.flash - dt * 6);
    });
}

// ---------------------------------------------------------------------------

float ease(Ease e, float t)
{
    t = clampf(t, 0, 1);
    switch (e) {
    case EASE_LINEAR: return t;
    case EASE_IN_QUAD: return t * t;
    case EASE_OUT_QUAD: return 1 - (1 - t) * (1 - t);
    case EASE_IN_OUT_QUAD: return t < 0.5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2;
    case EASE_OUT_CUBIC: return 1 - powf(1 - t, 3);
    case EASE_OUT_BACK: {
        const float c1 = 1.70158f, c3 = c1 + 1;
        return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
    }
    case EASE_OUT_ELASTIC:
        if (t == 0 || t == 1)
            return t;
        return powf(2, -10 * t) * sinf((t * 10 - 0.75f) * (6.2831853f / 3)) + 1;
    case EASE_OUT_BOUNCE: {
        const float n1 = 7.5625f, d1 = 2.75f;
        if (t < 1 / d1) return n1 * t * t;
        if (t < 2 / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
        if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
    }
    return t;
}

struct Tween {
    float* target;
    float from, to;
    float duration, time, delay;
    Ease ease;
    bool started;
};

static std::vector<Tween> tweens;

void tween(float* target, float to, float duration, Ease e, float delay)
{
    tween_cancel(target);
    tweens.push_back({ target, *target, to, duration, 0, delay, e, false });
}

void tween_cancel(float* target)
{
    for (size_t i = 0; i < tweens.size();) {
        if (tweens[i].target == target) {
            tweens[i] = tweens.back();
            tweens.pop_back();
        } else {
            i++;
        }
    }
}

bool tween_running(float* target)
{
    for (const Tween& t : tweens)
        if (t.target == target)
            return true;
    return false;
}

void tween_update(float dt)
{
    for (size_t i = 0; i < tweens.size();) {
        Tween& t = tweens[i];
        if (t.delay > 0) {
            t.delay -= dt;
            i++;
            continue;
        }
        if (!t.started) {
            // pick up the value as it is when we actually start
            t.from = *t.target;
            t.started = true;
        }
        t.time += dt;
        float k = t.duration > 0 ? t.time / t.duration : 1;
        *t.target = t.from + (t.to - t.from) * ease(t.ease, k);
        if (k >= 1) {
            *t.target = t.to;
            tweens[i] = tweens.back();
            tweens.pop_back();
        } else {
            i++;
        }
    }
}

void tween_clear()
{
    tweens.clear();
}
