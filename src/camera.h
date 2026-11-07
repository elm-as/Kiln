#pragma once
#include "vec.h"

// A side scroller camera: follows a point with some lag, stays inside the
// level, shakes, and can be pulled towards something (a boss) for a frame.
// pos is the top left corner of the view, in world pixels.

struct Camera {
    vec2 pos, prev;
    vec2 size;                  // the view, in world pixels
    Rect bounds;                // stays inside this; w or h of 0 means no limit that way
    float follow_x = 5, follow_y = 4;   // how quickly it catches up, per second
    float shake = 0;
    float shake_decay = 14;     // shake lost per second
    float look_weight = 0.4f;   // how far a camera_look() pulls it

    vec2 look_at;
    bool looking = false;       // cleared by every camera_update
};

vec2 camera_target(const Camera& c, vec2 focus);    // where it wants to be, clamped
void camera_snap(Camera& c, vec2 focus);            // straight there, no easing
void camera_update(Camera& c, vec2 focus, float dt);
void camera_shake(Camera& c, float amount);         // keeps the bigger of the two
void camera_look(Camera& c, vec2 at);               // pull towards this, this update only
vec2 camera_view(const Camera& c, float alpha);     // interpolated, shaken, on whole pixels
