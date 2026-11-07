#pragma once
#include <vector>
#include "components.h"
#include "tilemap.h"

// Platformer physics. Bodies are boxes or circles, never rotate, and the
// tilemap is always the last word: whatever the body-body pass does, a body
// ends the tick outside solid tiles.
//
// Two colliders touch when either one's layer is in the other's mask. Both
// ways round and they push each other; one way only and it's just reported,
// like a trigger. That's how the player can feel an enemy without shoving it.

struct Contact {
    Entity a, b;
    vec2 normal;        // from a to b
    float depth;
    bool trigger;
};

struct PhysicsConfig {
    float gravity = 900;
};

// Map edges: left and right are walls, the top is open (you can jump above
// it) and the bottom is a hole.
void physics_step(World& w, const Tilemap& map, const Tileset& ts, const PhysicsConfig& cfg,
                  float dt, std::vector<Contact>& contacts);

Rect collider_rect(const Transform& t, const Collider& c);
bool solid_at(const Tilemap& map, const Tileset& ts, vec2 p);
bool rect_hits_solid(const Tilemap& map, const Tileset& ts, Rect r);
bool rect_hits_hazard(const Tilemap& map, const Tileset& ts, Rect r);
// all the flags of the tiles whose hitbox overlaps r
uint8_t rect_tile_flags(const Tilemap& map, const Tileset& ts, Rect r);

struct RayHit {
    bool hit = false;
    float dist = 0;
    vec2 point;
    vec2 normal;
    Entity entity = NO_ENTITY;  // NO_ENTITY when it hit the map
};

RayHit raycast_tiles(const Tilemap& map, const Tileset& ts, vec2 from, vec2 dir, float max_dist);
RayHit raycast(World& w, const Tilemap& map, const Tileset& ts, vec2 from, vec2 dir, float max_dist,
               uint16_t mask = 0xffff, Entity ignore = NO_ENTITY);
void query_rect(World& w, Rect r, uint16_t mask, std::vector<Entity>& out);
