#pragma once
#include <vector>
#include "json.h"
#include "render.h"

// Background (and foreground) layers that scroll slower or faster than the
// camera. Each one is a sprite repeated side by side. In a scene's props:
//
//   "parallax": [
//     { "sprite": "bg_far",  "speed": 0.1, "anchor": "bottom", "offset": 20 },
//     { "sprite": "fg_roots", "speed": 1.3, "anchor": "top", "front": true }
//   ]
//
// speed 0 is glued to the screen, 1 moves with the world, above 1 is closer
// than the play area. anchor pins it to the top or bottom of the screen,
// offset pushes it inward. gap leaves room between repeats, drift scrolls it
// on its own (pixels per second), tint colors it.
struct ParallaxLayer {
    int sprite = -1;
    int frame = 0;
    float speed = 0.5f;
    float speed_y = -1;     // default: a fraction of speed, vertical parallax is subtle
    bool top = false;
    float offset = 0;
    float gap = 0;
    float drift = 0;
    bool front = false;
    Color tint = WHITE;
};

std::vector<ParallaxLayer> parallax_load(const Json& layers);

// Draws in canvas coordinates, call with the view reset. cam is the camera's
// top left in the world, world_h the level height so vertical parallax can be
// centered on it.
void parallax_draw(const std::vector<ParallaxLayer>& layers, bool front, vec2 cam, vec2 view, float world_h, double time);
