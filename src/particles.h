#pragma once
#include <string>
#include "components.h"

// Effects are defined in data/particles.json:
//
//   "dust": { "count": 6, "life": [0.3, 0.6], "speed": [10, 40], "angle": -90,
//             "spread": 60, "gravity": -20, "drag": 2, "size": [2, 0],
//             "color": ["#e0d8c8", "#e0d8c880"], "additive": false, "rate": 20,
//             "area": [4, 0] }
//
// angle and spread are in degrees, 0 is right and -90 is up. size and color
// go from the first value to the second over the particle's life.

void particles_load(const Json& j);
int particle_fx(const char* name);     // -1 if unknown
const char* particle_fx_name(int fx);  // "" if unknown
void particles_burst(int fx, vec2 pos, int count = -1, float angle = 1e9f);
void particles_update(World& w, float dt);     // also runs Emitter components
void particles_draw(bool additive);
void particles_clear();
int particles_alive();
