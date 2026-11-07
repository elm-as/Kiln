#pragma once
#include <vector>
#include "kiln.h"

// Thermal puzzle & physics systems:
// - Steam vents (upward propulsion, especially when gliding)
// - Dynamic thermal transfer and melting

struct SteamVent {
    vec2 pos;
    float width = 32.0f;
    float height = 96.0f;
    float power = 360.0f;
    bool active = true;
};

void thermal_init();
void thermal_reset();

/**
 * Registers a vertical steam vent column in the active scene.
 * @param pos Base center position of the vent in world coordinates.
 * @param w Width of the column in pixels.
 * @param h Height of the column extending upward from `pos.y`.
 * @param power Upward acceleration force applied inside the column.
 * @param active Initial activation state.
 * @return 0-based index of the registered vent.
 */
int thermal_add_vent(vec2 pos, float w, float h, float power, bool active = true);
void thermal_set_vent_active(int id, bool active);

// Updates steam vents: emits particles and applies lift to the player
void thermal_update(float dt);

/**
 * Computes the upward lift acceleration at world position `pos` across all active steam vents.
 * Multiplies lift when `gliding` (1.35x, or 1.85x if the "plume_suie" relic is equipped).
 * @param pos World position of the entity.
 * @param gliding Whether the player is currently gliding.
 * @return Upward acceleration in px/s^2 (0 if outside all active vents).
 */
float thermal_vent_lift_at(vec2 pos, bool gliding);
