#pragma once
#include "scene.h"
#include "script.h"
#include "tilemap.h"

// The functions every game's scripts get, acting on one scene:
//
//   x(e) y(e) set_pos(e, x, y)            vel_x(e) vel_y(e) set_vel(e, vx, vy)
//   set_gravity(e, g) on_ground(e) on_wall(e) ground_of(e)
//   anim(e, name, restart) anim_done(e) flip(e, on) flash(e) visible(e, on)
//   light(e, radius) emit(e, on) collide(e, on) alive(e)
//   spawn(prefab, x, y) destroy(e) find(name) dist(a, b)
//   solid(x, y) floor_at(x, y) sfx(name, volume, pitch) burst(fx, x, y, count)
//   hitstop(seconds) flag(name) set_flag(name, on)
//
// Entities are numbers; one that's gone reads as 0 instead of an error, so
// a bat chasing a player who just died doesn't have to care. The game adds
// its own functions after script_api_init, and can replace any of these by
// defining the same name again.

void script_api_init(Scene* scene, const Tileset* tileset);
void script_api_reset();        // when a level starts: parked colliders, flags

// for the game's own natives
World& script_world();
Entity script_arg_entity(const Value* a, int n, int i);    // alive, or NO_ENTITY
float script_argf(VM& vm, const Value* a, int n, int i, float def);
bool script_argb(const Value* a, int n, int i, bool def);
