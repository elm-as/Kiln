#pragma once
#include "components.h"

// sprite animation
void anim_play(World& w, Entity e, const char* name, bool restart = false);
bool anim_is(World& w, Entity e, const char* name);
void anim_update(World& w, float dt);

// tweens drive a float somewhere towards a value. The pointer has to stay
// valid for the whole tween, so aim these at globals and game state, not at
// components (their storage moves).
enum Ease {
    EASE_LINEAR, EASE_IN_QUAD, EASE_OUT_QUAD, EASE_IN_OUT_QUAD,
    EASE_OUT_CUBIC, EASE_OUT_BACK, EASE_OUT_ELASTIC, EASE_OUT_BOUNCE,
};

float ease(Ease e, float t);
void tween(float* target, float to, float duration, Ease e = EASE_OUT_QUAD, float delay = 0);
void tween_cancel(float* target);
bool tween_running(float* target);
void tween_update(float dt);
void tween_clear();
